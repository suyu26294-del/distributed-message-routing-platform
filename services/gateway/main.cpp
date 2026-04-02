#include "dist_platform/base64.hpp"
#include "dist_platform/config.hpp"
#include "dist_platform/logging.hpp"
#include "dist_platform/random_id.hpp"
#include "dist_platform/rpc_client.hpp"
#include "dist_platform/tcp_frame_server.hpp"
#include "dist_platform/time_utils.hpp"
#include "dist_platform.pb.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

using dist_platform::Config;
using dist_platform::TcpConnection;
using dist_platform::TcpFrameServer;
using json = nlohmann::json;

namespace {

void SendJson(const std::shared_ptr<TcpConnection>& connection, const json& payload) {
  connection->SendFrame(payload.dump());
}

json AckJson(const std::string& message, bool ok = true) {
  return json{{"ok", ok}, {"message", message}};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: gateway_service <config.json>\n";
    return 1;
  }

  try {
    dist_platform::InitLogging("gateway_service");
    const Config config = Config::LoadFile(argv[1]);
    const std::string gateway_host = config.GetString("host");
    const uint16_t debug_port = config.GetUint16("debug_port");
    const std::string instance_id = config.GetString("instance_id");
    const uint32_t weight = config.GetUint32("weight");

    const std::string session_host = config.GetString("session_host");
    const uint16_t session_port = config.GetUint16("session_port");
    const std::string router_host = config.GetString("router_host");
    const uint16_t router_port = config.GetUint16("router_port");
    const std::string file_host = config.GetString("file_host");
    const uint16_t file_port = config.GetUint16("file_port");

    std::mutex clients_mutex;
    std::unordered_map<std::string, std::shared_ptr<TcpConnection>> clients;
    std::atomic<bool> running{true};

    distplatform::RouterRequest register_request;
    register_request.mutable_register_service()->set_service_name("gateway");
    register_request.mutable_register_service()->set_instance_id(instance_id);
    register_request.mutable_register_service()->set_host(gateway_host);
    register_request.mutable_register_service()->set_port(debug_port);
    register_request.mutable_register_service()->set_weight(weight);
    auto register_response = dist_platform::CallRpc<distplatform::RouterRequest, distplatform::RouterResponse>(
        router_host, router_port, register_request);
    if (!register_response.ok()) {
      throw std::runtime_error(register_response.message());
    }

    std::thread load_reporter([&]() {
      while (running) {
        try {
          distplatform::RouterRequest report_request;
          report_request.mutable_load_report()->set_service_name("gateway");
          report_request.mutable_load_report()->set_instance_id(instance_id);
          size_t connection_count = 0;
          {
            std::lock_guard<std::mutex> lock(clients_mutex);
            connection_count = clients.size();
          }
          report_request.mutable_load_report()->set_active_connections(static_cast<uint32_t>(connection_count));
          report_request.mutable_load_report()->set_inflight_tasks(0);
          report_request.mutable_load_report()->set_load_score(connection_count / static_cast<double>(weight == 0 ? 1 : weight));
          dist_platform::CallRpc<distplatform::RouterRequest, distplatform::RouterResponse>(router_host, router_port, report_request);
        } catch (const std::exception& ex) {
          spdlog::warn("failed to report load: {}", ex.what());
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
      }
    });

    TcpFrameServer server(
        gateway_host,
        debug_port,
        8,
        [&](const std::shared_ptr<TcpConnection>& connection, const std::string& payload) {
          json request;
          try {
            request = json::parse(payload);
          } catch (const std::exception&) {
            SendJson(connection, AckJson("invalid json payload", false));
            return;
          }

          const std::string command = request.value("cmd", "");
          try {
            if (command == "login") {
              distplatform::SessionRequest session_request;
              auto* login = session_request.mutable_login();
              login->set_user_id(request.at("user_id").get<std::string>());
              login->set_gateway_instance_id(instance_id);
              login->set_gateway_host(gateway_host);
              login->set_gateway_port(debug_port);
              const auto response = dist_platform::CallRpc<distplatform::SessionRequest, distplatform::SessionResponse>(
                  session_host, session_port, session_request);
              if (!response.ok()) {
                throw std::runtime_error(response.message());
              }
              connection->SetTag("user_id", request.at("user_id").get<std::string>());
              connection->SetTag("session_id", response.login_response().session_id());
              {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients[request.at("user_id").get<std::string>()] = connection;
              }
              SendJson(connection,
                       json{{"ok", true},
                            {"type", "login"},
                            {"user_id", request.at("user_id").get<std::string>()},
                            {"session_id", response.login_response().session_id()},
                            {"gateway_instance_id", instance_id}});
              return;
            }

            if (command == "heartbeat") {
              distplatform::SessionRequest heartbeat_request;
              auto* heartbeat = heartbeat_request.mutable_heartbeat();
              heartbeat->set_user_id(connection->GetTag("user_id"));
              heartbeat->set_session_id(connection->GetTag("session_id"));
              heartbeat->set_gateway_instance_id(instance_id);
              const auto response = dist_platform::CallRpc<distplatform::SessionRequest, distplatform::SessionResponse>(
                  session_host, session_port, heartbeat_request);
              SendJson(connection, json{{"ok", response.ok()}, {"type", "heartbeat"}, {"message", response.message()}});
              return;
            }

            if (command == "send_message") {
              distplatform::RouteEnvelope envelope;
              envelope.set_message_id(dist_platform::RandomId("msg"));
              envelope.set_from_user(request.at("from_user").get<std::string>());
              envelope.set_to_user(request.at("to_user").get<std::string>());
              envelope.set_body(request.at("body").get<std::string>());
              envelope.set_created_at_ms(dist_platform::NowMillis());
              envelope.set_require_ack(true);

              distplatform::RouterRequest route_query;
              route_query.mutable_route_query()->set_target_user(envelope.to_user());
              const auto route_response = dist_platform::CallRpc<distplatform::RouterRequest, distplatform::RouterResponse>(
                  router_host, router_port, route_query);
              if (!route_response.ok()) {
                throw std::runtime_error(route_response.message());
              }

              bool delivered_live = false;
              {
                std::lock_guard<std::mutex> lock(clients_mutex);
                const auto it = clients.find(envelope.to_user());
                if (it != clients.end()) {
                  SendJson(it->second,
                           json{{"ok", true},
                                {"type", "message"},
                                {"message_id", envelope.message_id()},
                                {"from_user", envelope.from_user()},
                                {"to_user", envelope.to_user()},
                                {"body", envelope.body()},
                                {"offline", false}});
                  delivered_live = true;
                }
              }
              if (!delivered_live) {
                distplatform::RouterRequest store_request;
                *store_request.mutable_route_envelope() = envelope;
                const auto store_response = dist_platform::CallRpc<distplatform::RouterRequest, distplatform::RouterResponse>(
                    router_host, router_port, store_request);
                if (!store_response.ok()) {
                  throw std::runtime_error(store_response.message());
                }
              }

              SendJson(connection,
                       json{{"ok", true},
                            {"type", "send_message"},
                            {"message_id", envelope.message_id()},
                            {"stored_offline", !delivered_live},
                            {"delivered_live", delivered_live}});
              return;
            }

            if (command == "pull_offline") {
              distplatform::RouterRequest pull_request;
              pull_request.mutable_offline_pull()->set_user_id(request.at("user_id").get<std::string>());
              const auto pull_response = dist_platform::CallRpc<distplatform::RouterRequest, distplatform::RouterResponse>(
                  router_host, router_port, pull_request);
              if (!pull_response.ok()) {
                throw std::runtime_error(pull_response.message());
              }
              json messages = json::array();
              for (const auto& message : pull_response.offline_batch().messages()) {
                messages.push_back(json{{"message_id", message.message_id()},
                                        {"from_user", message.from_user()},
                                        {"to_user", message.to_user()},
                                        {"body", message.body()},
                                        {"offline", true}});
              }
              SendJson(connection, json{{"ok", true}, {"type", "offline_batch"}, {"messages", messages}});
              return;
            }

            if (command == "file_manifest") {
              distplatform::FileRequest file_request;
              auto* manifest = file_request.mutable_manifest();
              const std::string transfer_id = request.value("transfer_id", dist_platform::RandomId("file"));
              manifest->set_transfer_id(transfer_id);
              manifest->set_owner_user(request.at("owner_user").get<std::string>());
              manifest->set_file_name(request.at("file_name").get<std::string>());
              manifest->set_file_size(request.at("file_size").get<uint64_t>());
              manifest->set_chunk_size(request.at("chunk_size").get<uint32_t>());
              manifest->set_total_chunks(request.at("total_chunks").get<uint32_t>());
              manifest->set_sha256(request.value("sha256", std::string()));
              const auto file_response = dist_platform::CallRpc<distplatform::FileRequest, distplatform::FileResponse>(
                  file_host, file_port, file_request);
              if (!file_response.ok()) {
                throw std::runtime_error(file_response.message());
              }
              SendJson(connection,
                       json{{"ok", true},
                            {"type", "file_manifest"},
                            {"transfer_id", file_response.manifest().transfer_id()},
                            {"total_chunks", file_response.manifest().total_chunks()}});
              return;
            }

            if (command == "file_chunk") {
              distplatform::FileRequest file_request;
              auto* chunk = file_request.mutable_chunk();
              chunk->set_transfer_id(request.at("transfer_id").get<std::string>());
              chunk->set_chunk_index(request.at("chunk_index").get<uint32_t>());
              chunk->set_data(dist_platform::Base64Decode(request.at("data_base64").get<std::string>()));
              chunk->set_eof(request.value("eof", false));
              const auto file_response = dist_platform::CallRpc<distplatform::FileRequest, distplatform::FileResponse>(
                  file_host, file_port, file_request);
              if (!file_response.ok()) {
                throw std::runtime_error(file_response.message());
              }
              SendJson(connection,
                       json{{"ok", true},
                            {"type", "file_chunk"},
                            {"transfer_id", file_response.chunk_ack().transfer_id()},
                            {"chunk_index", file_response.chunk_ack().chunk_index()},
                            {"received_chunks", file_response.chunk_ack().received_chunks()}});
              return;
            }

            if (command == "query_resume") {
              distplatform::FileRequest file_request;
              file_request.mutable_resume_query()->set_transfer_id(request.at("transfer_id").get<std::string>());
              const auto file_response = dist_platform::CallRpc<distplatform::FileRequest, distplatform::FileResponse>(
                  file_host, file_port, file_request);
              if (!file_response.ok()) {
                throw std::runtime_error(file_response.message());
              }
              json missing = json::array();
              for (const auto chunk_index : file_response.resume_token().missing_chunks()) {
                missing.push_back(chunk_index);
              }
              SendJson(connection,
                       json{{"ok", true},
                            {"type", "resume_token"},
                            {"transfer_id", file_response.resume_token().transfer_id()},
                            {"next_chunk", file_response.resume_token().next_chunk()},
                            {"missing_chunks", missing}});
              return;
            }

            if (command == "download_chunk") {
              distplatform::FileRequest file_request;
              file_request.mutable_download()->set_transfer_id(request.at("transfer_id").get<std::string>());
              file_request.mutable_download()->set_chunk_index(request.at("chunk_index").get<uint32_t>());
              const auto file_response = dist_platform::CallRpc<distplatform::FileRequest, distplatform::FileResponse>(
                  file_host, file_port, file_request);
              if (!file_response.ok()) {
                throw std::runtime_error(file_response.message());
              }
              SendJson(connection,
                       json{{"ok", true},
                            {"type", "download_chunk"},
                            {"transfer_id", file_response.chunk().transfer_id()},
                            {"chunk_index", file_response.chunk().chunk_index()},
                            {"eof", file_response.chunk().eof()},
                            {"data_base64", dist_platform::Base64Encode(file_response.chunk().data())}});
              return;
            }

            if (command == "summary") {
              distplatform::RouterRequest router_request;
              router_request.mutable_summary();
              const auto router_response = dist_platform::CallRpc<distplatform::RouterRequest, distplatform::RouterResponse>(
                  router_host, router_port, router_request);
              if (!router_response.ok()) {
                throw std::runtime_error(router_response.message());
              }
              SendJson(connection,
                       json{{"ok", true},
                            {"type", "summary"},
                            {"online_users", router_response.summary().online_users()},
                            {"stored_offline_messages", router_response.summary().stored_offline_messages()},
                            {"registered_gateways", router_response.summary().registered_gateways()},
                            {"active_transfers", router_response.summary().active_transfers()}});
              return;
            }

            SendJson(connection, AckJson("unsupported command", false));
          } catch (const std::exception& ex) {
            SendJson(connection, json{{"ok", false}, {"message", ex.what()}, {"type", command}});
          }
        },
        [&](const std::shared_ptr<TcpConnection>& connection) {
          const std::string user_id = connection->GetTag("user_id");
          if (user_id.empty()) {
            return;
          }
          std::lock_guard<std::mutex> lock(clients_mutex);
          const auto it = clients.find(user_id);
          if (it != clients.end() && it->second == connection) {
            clients.erase(it);
          }
        });

    server.Start();
    spdlog::info("gateway_service listening on {}:{}", gateway_host, debug_port);
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    running = false;
    load_reporter.join();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}