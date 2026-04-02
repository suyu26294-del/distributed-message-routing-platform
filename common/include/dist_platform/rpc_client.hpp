#pragma once

#include "dist_platform/frame_codec.hpp"

#include <chrono>
#include <stdexcept>
#include <string>

namespace dist_platform {

std::string CallRawRpc(const std::string& host,
                       uint16_t port,
                       const std::string& payload,
                       std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

template <typename Request, typename Response>
Response CallRpc(const std::string& host,
                 uint16_t port,
                 const Request& request,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(3000)) {
  const std::string payload = request.SerializeAsString();
  const std::string response_payload = CallRawRpc(host, port, payload, timeout);
  Response response;
  if (!response.ParseFromString(response_payload)) {
    throw std::runtime_error("failed to parse rpc response");
  }
  return response;
}

}  // namespace dist_platform