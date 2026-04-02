#include "dist_platform/rpc_client.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace dist_platform {
namespace {

void SendAll(int fd, const std::string& data) {
  size_t total = 0;
  while (total < data.size()) {
    const ssize_t written = send(fd, data.data() + total, data.size() - total, MSG_NOSIGNAL);
    if (written < 0) {
      throw std::runtime_error(std::string("send failed: ") + std::strerror(errno));
    }
    total += static_cast<size_t>(written);
  }
}

std::string RecvExact(int fd, size_t size) {
  std::string data(size, '\0');
  size_t total = 0;
  while (total < size) {
    const ssize_t received = recv(fd, data.data() + total, size - total, 0);
    if (received <= 0) {
      throw std::runtime_error("recv failed or closed");
    }
    total += static_cast<size_t>(received);
  }
  return data;
}

}  // namespace

std::string CallRawRpc(const std::string& host, uint16_t port, const std::string& payload, std::chrono::milliseconds timeout) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    throw std::runtime_error("socket failed");
  }

  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout.count() / 1000);
  tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
    close(fd);
    throw std::runtime_error("inet_pton failed");
  }
  if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    const std::string error = std::strerror(errno);
    close(fd);
    throw std::runtime_error("connect failed: " + error);
  }

  SendAll(fd, EncodeFrame(payload));
  const std::string header = RecvExact(fd, sizeof(uint32_t));
  uint32_t length = 0;
  std::memcpy(&length, header.data(), sizeof(length));
  length = ntohl(length);
  const std::string response = RecvExact(fd, length);
  close(fd);
  return response;
}

}  // namespace dist_platform