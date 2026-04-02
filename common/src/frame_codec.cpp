#include "dist_platform/frame_codec.hpp"

#include <arpa/inet.h>
#include <cstring>

namespace dist_platform {

std::string EncodeFrame(std::string_view payload) {
  std::string frame(sizeof(uint32_t), '\0');
  const uint32_t length = htonl(static_cast<uint32_t>(payload.size()));
  std::memcpy(frame.data(), &length, sizeof(length));
  frame.append(payload.data(), payload.size());
  return frame;
}

bool TryDecodeFrame(std::string& buffer, std::string& payload) {
  if (buffer.size() < sizeof(uint32_t)) {
    return false;
  }
  uint32_t length = 0;
  std::memcpy(&length, buffer.data(), sizeof(length));
  length = ntohl(length);
  if (buffer.size() < sizeof(uint32_t) + length) {
    return false;
  }
  payload.assign(buffer.data() + sizeof(uint32_t), length);
  buffer.erase(0, sizeof(uint32_t) + length);
  return true;
}

}  // namespace dist_platform