#include "dist_platform/config.hpp"
#include "dist_platform/logging.hpp"
#include "dist_platform/metadata_repository.hpp"
#include "dist_platform/mysql_store.hpp"
#include "dist_platform/tcp_frame_server.hpp"
#include "dist_platform.pb.h"

#include <spdlog/spdlog.h>

#include <exception>
#include <iostream>

using dist_platform::Config;
using dist_platform::MetadataRepository;
using dist_platform::MySqlStore;
using dist_platform::TcpConnection;
using dist_platform::TcpFrameServer;

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: metadata_service <config.json>\n";
    return 1;
  }

  try {
    dist_platform::InitLogging("metadata_service");
    const Config config = Config::LoadFile(argv[1]);

    MySqlStore store(config.GetString("mysql_host"),
                     config.GetUint16("mysql_port"),
                     config.GetString("mysql_user"),
                     config.GetString("mysql_password"),
                     config.GetString("mysql_database"));
    store.Connect();
    MetadataRepository repository(store);
    repository.EnsureSchema();

    TcpFrameServer server(
        config.GetString("listen_host"),
        config.GetUint16("listen_port"),
        4,
        [&repository](const std::shared_ptr<TcpConnection>& connection, const std::string& payload) {
          distplatform::MetadataRequest request;
          distplatform::MetadataResponse response;
          if (!request.ParseFromString(payload)) {
            response.set_ok(false);
            response.set_message("invalid metadata request");
            connection->SendFrame(response.SerializeAsString());
            return;
          }

          try {
            switch (request.payload_case()) {
              case distplatform::MetadataRequest::kEnsureUser:
                repository.EnsureUser(request.ensure_user().user_id());
                response.set_ok(true);
                response.set_message("user ensured");
                break;
              case distplatform::MetadataRequest::kPersistMessage:
                repository.PersistMessage(request.persist_message().envelope(), request.persist_message().state());
                response.set_ok(true);
                response.set_message("message persisted");
                break;
              case distplatform::MetadataRequest::kUpdateMessage:
                repository.UpdateMessageState(request.update_message().message_id(), request.update_message().state());
                response.set_ok(true);
                response.set_message("message state updated");
                break;
              case distplatform::MetadataRequest::kPersistFile:
                repository.PersistFile(request.persist_file().manifest(), request.persist_file().state());
                response.set_ok(true);
                response.set_message("file manifest persisted");
                break;
              case distplatform::MetadataRequest::kRecordChunk:
                repository.RecordChunk(request.record_chunk().transfer_id(),
                                       request.record_chunk().chunk_index(),
                                       request.record_chunk().chunk_size());
                response.set_ok(true);
                response.set_message("chunk recorded");
                break;
              case distplatform::MetadataRequest::kQueryTransfer: {
                const auto transfer = repository.QueryTransfer(request.query_transfer().transfer_id());
                response.set_ok(transfer.found);
                response.set_message(transfer.found ? "transfer found" : "transfer not found");
                if (transfer.found) {
                  *response.mutable_manifest() = transfer.manifest;
                  for (const auto chunk_index : transfer.chunk_indices) {
                    response.add_chunk_indices(chunk_index);
                  }
                }
                break;
              }
              case distplatform::MetadataRequest::kSummary: {
                const auto counts = repository.SummaryCounts();
                response.set_ok(true);
                response.set_stored_offline_messages(counts.stored_offline_messages);
                response.set_active_transfers(counts.active_transfers);
                response.set_message("summary ready");
                break;
              }
              default:
                response.set_ok(false);
                response.set_message("unsupported metadata operation");
                break;
            }
          } catch (const std::exception& ex) {
            response.set_ok(false);
            response.set_message(ex.what());
          }

          connection->SendFrame(response.SerializeAsString());
        });

    server.Start();
    spdlog::info("metadata_service listening on {}:{}", config.GetString("listen_host"), config.GetUint16("listen_port"));
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}