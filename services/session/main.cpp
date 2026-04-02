#include "dist_platform/config.hpp"
#include "dist_platform/logging.hpp"
#include "dist_platform/random_id.hpp"
#include "dist_platform/redis_client.hpp"
#include "dist_platform/rpc_client.hpp"
#include "dist_platform/tcp_frame_server.hpp"
#include "dist_platform/time_utils.hpp"
#include "dist_platform.pb.h"

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
    std::cerr << "usage: session_service <config.json>\n";
    return 1;
  }

  try {
    dist_platform::InitLogging("session_service");
    const Config config = Config::LoadFile(argv[1]);
    RedisClient redis(config.GetString("redis_host"), config.GetInt("redis_port"));
    redis.Connect();
    redis.Ping();

    const std::string metadata_host = config.GetString("metadata_host");
    const uint16_t metadata_port = config.GetUint16("metadata_port");
    const int ttl_seconds = config.GetInt("session_ttl_seconds");

    TcpFrameServer server(
        config.GetString("listen_host"),
        config.GetUint16("listen_port"),
        4,
        [&](const std::shared_ptr<TcpConnection>& connection, const std::string& payload) {
          distplatform::SessionRequest request;
          distplatform::SessionResponse response;
          if (!request.ParseFromString(payload)) {
            response.set_ok(false);
            response.set_message("invalid session request");
            connection->SendFrame(response.SerializeAsString());
            return;
          }

          try {
            switch (request.payload_case()) {
              case distplatform::SessionRequest::kLogin: {
                distplatform::MetadataRequest metadata_request;
                metadata_request.mutable_ensure_user()->set_user_id(request.login().user_id());
                auto metadata_response = dist_platform::CallRpc<distplatform::MetadataRequest, distplatform::MetadataResponse>(
                    metadata_host, metadata_port, metadata_request);
                if (!metadata_response.ok()) {
                  throw std::runtime_error(metadata_response.message());
                }

                const std::string session_id = dist_platform::RandomId("sess");
                const std::string key = "session:" + request.login().user_id();
                redis.HSet(key,
                           {{"session_id", session_id},
                            {"gateway_instance_id", request.login().gateway_instance_id()},
                            {"gateway_host", request.login().gateway_host()},
                            {"gateway_port", std::to_string(request.login().gateway_port())},
                            {"updated_at_ms", std::to_string(dist_platform::NowMillis())}});
                redis.Expire(key, ttl_seconds);

                response.set_ok(true);
                response.set_message("login accepted");
                auto* login_response = response.mutable_login_response();
                login_response->set_ok(true);
                login_response->set_session_id(session_id);
                login_response->set_gateway_instance_id(request.login().gateway_instance_id());
                login_response->set_message("session created");
                break;
              }
              case distplatform::SessionRequest::kHeartbeat: {
                const std::string key = "session:" + request.heartbeat().user_id();
                redis.HSet(key,
                           {{"session_id", request.heartbeat().session_id()},
                            {"gateway_instance_id", request.heartbeat().gateway_instance_id()},
                            {"updated_at_ms", std::to_string(dist_platform::NowMillis())}});
                redis.Expire(key, ttl_seconds);
                response.set_ok(true);
                response.set_message("heartbeat accepted");
                break;
              }
              default:
                response.set_ok(false);
                response.set_message("unsupported session operation");
                break;
            }
          } catch (const std::exception& ex) {
            response.set_ok(false);
            response.set_message(ex.what());
          }

          connection->SendFrame(response.SerializeAsString());
        });

    server.Start();
    spdlog::info("session_service listening on {}:{}", config.GetString("listen_host"), config.GetUint16("listen_port"));
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}