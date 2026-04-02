#include "dist_platform/config.hpp"
#include "dist_platform/file_utils.hpp"
#include "dist_platform/logging.hpp"
#include "dist_platform/rpc_client.hpp"
#include "dist_platform/tcp_frame_server.hpp"
#include "dist_platform.pb.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
#include <vector>

using dist_platform::Config;
using dist_platform::TcpConnection;
using dist_platform::TcpFrameServer;

namespace {

distplatform::ResumeToken BuildResume(const distplatform::FileManifest& manifest, const std::vector<uint32_t>& existing_chunks) {
  distplatform::ResumeToken token;
  token.set_transfer_id(manifest.transfer_id());
  uint32_t next_chunk = manifest.total_chunks();
  size_t existing_index = 0;
  for (uint32_t chunk = 0; chunk < manifest.total_chunks(); ++chunk) {
    if (existing_index < existing_chunks.size() && existing_chunks[existing_index] == chunk) {
      ++existing_index;
      continue;
    }
    token.add_missing_chunks(chunk);
    if (next_chunk == manifest.total_chunks()) {
      next_chunk = chunk;
    }
  }
  token.set_next_chunk(next_chunk == manifest.total_chunks() ? manifest.total_chunks() : next_chunk);
  return token;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: file_service <config.json>\n";
    return 1;
  }

  try {
    dist_platform::InitLogging("file_service");
    const Config config = Config::LoadFile(argv[1]);
    const std::string metadata_host = config.GetString("metadata_host");
    const uint16_t metadata_port = config.GetUint16("metadata_port");
    const std::string data_dir = config.GetString("data_dir");
    dist_platform::EnsureDir(data_dir);

    TcpFrameServer server(
        config.GetString("listen_host"),
        config.GetUint16("listen_port"),
        4,
        [&](const std::shared_ptr<TcpConnection>& connection, const std::string& payload) {
          distplatform::FileRequest request;
          distplatform::FileResponse response;
          if (!request.ParseFromString(payload)) {
            response.set_ok(false);
            response.set_message("invalid file request");
            connection->SendFrame(response.SerializeAsString());
            return;
          }

          try {
            switch (request.payload_case()) {
              case distplatform::FileRequest::kManifest: {
                distplatform::FileManifest manifest = request.manifest();
                if (manifest.total_chunks() == 0 && manifest.chunk_size() > 0) {
                  manifest.set_total_chunks(dist_platform::ChunkCountFromSize(manifest.file_size(), manifest.chunk_size()));
                }
                distplatform::MetadataRequest metadata_request;
                auto* persist_file = metadata_request.mutable_persist_file();
                *persist_file->mutable_manifest() = manifest;
                persist_file->set_state("in_progress");
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, metadata_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                response.set_ok(true);
                response.set_message("manifest accepted");
                *response.mutable_manifest() = manifest;
                break;
              }
              case distplatform::FileRequest::kChunk: {
                distplatform::MetadataRequest transfer_query;
                transfer_query.mutable_query_transfer()->set_transfer_id(request.chunk().transfer_id());
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, transfer_query);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                const auto& manifest = metadata_response.manifest();
                const std::string part_path = dist_platform::PartPath(data_dir, manifest.transfer_id());
                dist_platform::WriteChunkAt(part_path, request.chunk().chunk_index(), manifest.chunk_size(), request.chunk().data());

                distplatform::MetadataRequest record_chunk;
                record_chunk.mutable_record_chunk()->set_transfer_id(request.chunk().transfer_id());
                record_chunk.mutable_record_chunk()->set_chunk_index(request.chunk().chunk_index());
                record_chunk.mutable_record_chunk()->set_chunk_size(static_cast<uint32_t>(request.chunk().data().size()));
                auto record_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, record_chunk);
                if (!record_response.ok()) {
                  throw std::runtime_error(record_response.message());
                }

                distplatform::MetadataRequest refresh_query;
                refresh_query.mutable_query_transfer()->set_transfer_id(request.chunk().transfer_id());
                auto refresh_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, refresh_query);
                if (!refresh_response.ok()) {
                  throw std::runtime_error(refresh_response.message());
                }

                if (refresh_response.chunk_indices_size() >= refresh_response.manifest().total_chunks()) {
                  const std::string final_path = dist_platform::FinalPath(data_dir, manifest.transfer_id(), manifest.file_name());
                  if (!dist_platform::FileExists(final_path)) {
                    dist_platform::FinalizeTransfer(part_path, final_path);
                  }
                }

                response.set_ok(true);
                response.set_message("chunk accepted");
                response.mutable_chunk_ack()->set_transfer_id(request.chunk().transfer_id());
                response.mutable_chunk_ack()->set_chunk_index(request.chunk().chunk_index());
                response.mutable_chunk_ack()->set_accepted(true);
                response.mutable_chunk_ack()->set_received_chunks(static_cast<uint32_t>(refresh_response.chunk_indices_size()));
                response.mutable_chunk_ack()->set_detail("stored");
                break;
              }
              case distplatform::FileRequest::kResumeQuery: {
                distplatform::MetadataRequest query_request;
                query_request.mutable_query_transfer()->set_transfer_id(request.resume_query().transfer_id());
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, query_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                std::vector<uint32_t> existing;
                existing.reserve(metadata_response.chunk_indices_size());
                for (const auto chunk_index : metadata_response.chunk_indices()) {
                  existing.push_back(chunk_index);
                }
                *response.mutable_resume_token() = BuildResume(metadata_response.manifest(), existing);
                response.set_ok(true);
                response.set_message("resume plan ready");
                break;
              }
              case distplatform::FileRequest::kDownload: {
                distplatform::MetadataRequest query_request;
                query_request.mutable_query_transfer()->set_transfer_id(request.download().transfer_id());
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, query_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                const auto& manifest = metadata_response.manifest();
                std::string file_path = dist_platform::FinalPath(data_dir, manifest.transfer_id(), manifest.file_name());
                if (!dist_platform::FileExists(file_path)) {
                  file_path = dist_platform::PartPath(data_dir, manifest.transfer_id());
                }
                std::string data = dist_platform::ReadChunkAt(file_path, request.download().chunk_index(), manifest.chunk_size());
                response.set_ok(true);
                response.set_message("chunk ready");
                response.mutable_chunk()->set_transfer_id(manifest.transfer_id());
                response.mutable_chunk()->set_chunk_index(request.download().chunk_index());
                response.mutable_chunk()->set_data(data);
                response.mutable_chunk()->set_eof(request.download().chunk_index() + 1 >= manifest.total_chunks());
                break;
              }
              case distplatform::FileRequest::kSummary: {
                distplatform::MetadataRequest metadata_request;
                metadata_request.mutable_summary();
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, metadata_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                response.set_ok(true);
                response.set_message("summary ready");
                response.mutable_summary()->set_active_transfers(metadata_response.active_transfers());
                break;
              }
              default:
                response.set_ok(false);
                response.set_message("unsupported file operation");
                break;
            }
          } catch (const std::exception& ex) {
            response.set_ok(false);
            response.set_message(ex.what());
          }

          connection->SendFrame(response.SerializeAsString());
        });

    server.Start();
    spdlog::info("file_service listening on {}:{}", config.GetString("listen_host"), config.GetUint16("listen_port"));
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}