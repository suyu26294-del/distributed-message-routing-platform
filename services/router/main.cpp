#include "dist_platform/base64.hpp"
#include "dist_platform/config.hpp"
#include "dist_platform/logging.hpp"
#include "dist_platform/redis_client.hpp"
#include "dist_platform/rpc_client.hpp"
#include "dist_platform/tcp_frame_server.hpp"
#include "dist_platform.pb.h"

#include <algorithm>
#include <spdlog/spdlog.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <thread>

using dist_platform::Config;
using dist_platform::RedisClient;
using dist_platform::TcpConnection;
using dist_platform::TcpFrameServer;

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: router_service <config.json>\n";
    return 1;
  }

  try {
    dist_platform::InitLogging("router_service");
    const Config config = Config::LoadFile(argv[1]);
    RedisClient redis(config.GetString("redis_host"), config.GetInt("redis_port"));
    redis.Connect();
    redis.Ping();

    const std::string metadata_host = config.GetString("metadata_host");
    const uint16_t metadata_port = config.GetUint16("metadata_port");

    TcpFrameServer server(
        config.GetString("listen_host"),
        config.GetUint16("listen_port"),
        4,
        [&](const std::shared_ptr<TcpConnection>& connection, const std::string& payload) {
          distplatform::RouterRequest request;
          distplatform::RouterResponse response;
          if (!request.ParseFromString(payload)) {
            response.set_ok(false);
            response.set_message("invalid router request");
            connection->SendFrame(response.SerializeAsString());
            return;
          }

          try {
            switch (request.payload_case()) {
              case distplatform::RouterRequest::kRegisterService: {
                const auto& register_service = request.register_service();
                redis.HSet("service:" + register_service.service_name() + ":" + register_service.instance_id(),
                           {{"host", register_service.host()},
                            {"port", std::to_string(register_service.port())},
                            {"weight", std::to_string(register_service.weight())},
                            {"load_score", "0"}});
                response.set_ok(true);
                response.set_message("service registered");
                break;
              }
              case distplatform::RouterRequest::kLoadReport: {
                const auto& report = request.load_report();
                redis.HSet("service:" + report.service_name() + ":" + report.instance_id(),
                           {{"active_connections", std::to_string(report.active_connections())},
                            {"inflight_tasks", std::to_string(report.inflight_tasks())},
                            {"load_score", std::to_string(report.load_score())}});
                response.set_ok(true);
                response.set_message("load updated");
                break;
              }
              case distplatform::RouterRequest::kRouteQuery: {
                const auto session = redis.HGetAll("session:" + request.route_query().target_user());
                response.set_ok(true);
                auto* decision = response.mutable_route_decision();
                const bool online = !session.empty() && session.find("gateway_host") != session.end();
                decision->set_online(online);
                if (online) {
                  decision->set_instance_id(session.at("gateway_instance_id"));
                  decision->set_host(session.at("gateway_host"));
                  decision->set_port(static_cast<uint32_t>(std::stoul(session.at("gateway_port"))));
                  decision->set_session_id(session.at("session_id"));
                }
                response.set_message(online ? "route found" : "user offline");
                break;
              }
              case distplatform::RouterRequest::kRouteEnvelope: {
                const auto& envelope = request.route_envelope();
                redis.LPush("offline:" + envelope.to_user(), dist_platform::Base64Encode(envelope.SerializeAsString()));

                distplatform::MetadataRequest metadata_request;
                auto* persist = metadata_request.mutable_persist_message();
                *persist->mutable_envelope() = envelope;
                persist->set_state("offline");
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, metadata_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }

                response.set_ok(true);
                response.set_message("offline message stored");
                response.mutable_ack_response()->set_message_id(envelope.message_id());
                response.mutable_ack_response()->set_user_id(envelope.to_user());
                response.mutable_ack_response()->set_accepted(true);
                response.mutable_ack_response()->set_detail("offline");
                break;
              }
              case distplatform::RouterRequest::kOfflinePull: {
                const std::string key = "offline:" + request.offline_pull().user_id();
                auto entries = redis.LRange(key, 0, -1);
                std::reverse(entries.begin(), entries.end());
                for (const auto& entry : entries) {
                  distplatform::RouteEnvelope envelope;
                  if (envelope.ParseFromString(dist_platform::Base64Decode(entry))) {
                    *response.mutable_offline_batch()->add_messages() = envelope;
                    distplatform::MetadataRequest update_request;
                    update_request.mutable_update_message()->set_message_id(envelope.message_id());
                    update_request.mutable_update_message()->set_state("delivered");
                    auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                        metadata_host, metadata_port, update_request);
                    if (!metadata_response.ok()) {
                      throw std::runtime_error(metadata_response.message());
                    }
                  }
                }
                redis.Del(key);
                response.set_ok(true);
                response.set_message("offline batch ready");
                break;
              }
              case distplatform::RouterRequest::kAck: {
                distplatform::MetadataRequest update_request;
                update_request.mutable_update_message()->set_message_id(request.ack().message_id());
                update_request.mutable_update_message()->set_state(request.ack().accepted() ? "acked" : "failed");
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, update_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                response.set_ok(true);
                response.set_message("ack recorded");
                *response.mutable_ack_response() = request.ack();
                break;
              }
              case distplatform::RouterRequest::kSummary: {
                distplatform::MetadataRequest metadata_request;
                metadata_request.mutable_summary();
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, metadata_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }
                response.set_ok(true);
                response.set_message("summary ready");
                response.mutable_summary()->set_online_users(static_cast<uint32_t>(redis.Keys("session:*").size()));
                uint32_t offline_count = 0;
                for (const auto& key : redis.Keys("offline:*")) {
                  offline_count += static_cast<uint32_t>(redis.LLen(key));
                }
                response.mutable_summary()->set_stored_offline_messages(offline_count);
                response.mutable_summary()->set_registered_gateways(static_cast<uint32_t>(redis.Keys("service:gateway:*").size()));
                response.mutable_summary()->set_active_transfers(metadata_response.active_transfers());
                break;
              }
              default:
                response.set_ok(false);
                response.set_message("unsupported router operation");
                break;
            }
          } catch (const std::exception& ex) {
            response.set_ok(false);
            response.set_message(ex.what());
          }

          connection->SendFrame(response.SerializeAsString());
        });

    server.Start();
    spdlog::info("router_service listening on {}:{}", config.GetString("listen_host"), config.GetUint16("listen_port"));
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}