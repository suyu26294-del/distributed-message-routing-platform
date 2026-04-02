#include "dist_platform/tcp_frame_server.hpp"

#include "dist_platform/frame_codec.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

namespace dist_platform {
namespace {

void SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    throw std::runtime_error("failed to set non-blocking fd");
  }
}

}  // namespace

TcpConnection::TcpConnection(int fd) : fd_(fd), closed_(false) {}

TcpConnection::~TcpConnection() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

int TcpConnection::Fd() const { return fd_; }

void TcpConnection::SendFrame(const std::string& payload) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) {
    return;
  }
  const std::string frame = EncodeFrame(payload);
  size_t total = 0;
  while (total < frame.size()) {
    const ssize_t written = send(fd_, frame.data() + total, frame.size() - total, MSG_NOSIGNAL);
    if (written < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
      closed_ = true;
      return;
    }
    total += static_cast<size_t>(written);
  }
}

void TcpConnection::MarkClosed() { closed_ = true; }
bool TcpConnection::IsClosed() const { return closed_; }

void TcpConnection::SetTag(const std::string& key, const std::string& value) {
  std::lock_guard<std::mutex> lock(mutex_);
  tags_[key] = value;
}

std::string TcpConnection::GetTag(const std::string& key) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = tags_.find(key);
  return it == tags_.end() ? std::string() : it->second;
}

TcpFrameServer::TcpFrameServer(std::string host,
                               uint16_t port,
                               size_t worker_count,
                               MessageHandler on_message,
                               DisconnectHandler on_disconnect)
    : host_(std::move(host)),
      port_(port),
      listen_fd_(-1),
      epoll_fd_(-1),
      on_message_(std::move(on_message)),
      on_disconnect_(std::move(on_disconnect)),
      workers_(worker_count),
      running_(false) {}

TcpFrameServer::~TcpFrameServer() { Stop(); }

void TcpFrameServer::Start() {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error("failed to create listen socket");
  }
  int enable = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
  SetNonBlocking(listen_fd_);

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port_);
  if (inet_pton(AF_INET, host_.c_str(), &address.sin_addr) != 1) {
    throw std::runtime_error("inet_pton failed for listen host");
  }
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    throw std::runtime_error(std::string("bind failed: ") + std::strerror(errno));
  }
  if (listen(listen_fd_, SOMAXCONN) != 0) {
    throw std::runtime_error("listen failed");
  }

  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    throw std::runtime_error("epoll_create1 failed");
  }
  epoll_event event{};
  event.events = EPOLLIN;
  event.data.fd = listen_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &event) != 0) {
    throw std::runtime_error("epoll_ctl add listen fd failed");
  }

  running_ = true;
  loop_thread_ = std::thread([this]() { EventLoop(); });
}

void TcpFrameServer::Stop() {
  if (!running_.exchange(false)) {
    return;
  }
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    listen_fd_ = -1;
  }
  if (loop_thread_.joinable()) {
    loop_thread_.join();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  connections_.clear();
  read_buffers_.clear();
}

size_t TcpFrameServer::ConnectionCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return connections_.size();
}

void TcpFrameServer::CloseConnection(int fd) {
  std::shared_ptr<TcpConnection> connection;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
      return;
    }
    connection = it->second;
    connection->MarkClosed();
    connections_.erase(it);
    read_buffers_.erase(fd);
  }
  if (epoll_fd_ >= 0) {
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
  }
  if (on_disconnect_) {
    on_disconnect_(connection);
  }
}

void TcpFrameServer::AcceptLoop() {
  while (true) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    const int client_fd = accept4(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len, SOCK_NONBLOCK);
    if (client_fd < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
      }
      spdlog::error("accept failed: {}", std::strerror(errno));
      return;
    }

    auto connection = std::make_shared<TcpConnection>(client_fd);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      connections_[client_fd] = connection;
      read_buffers_[client_fd] = std::string();
    }
    epoll_event event{};
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
    event.data.fd = client_fd;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &event);
  }
}

void TcpFrameServer::EventLoop() {
  std::vector<epoll_event> events(32);
  while (running_) {
    const int ready = epoll_wait(epoll_fd_, events.data(), static_cast<int>(events.size()), 500);
    if (ready < 0) {
      if (!running_ || errno == EINTR) {
        continue;
      }
      spdlog::error("epoll_wait failed: {}", std::strerror(errno));
      continue;
    }
    for (int index = 0; index < ready; ++index) {
      const int fd = events[index].data.fd;
      const uint32_t flags = events[index].events;
      if (fd == listen_fd_) {
        AcceptLoop();
        continue;
      }
      if ((flags & EPOLLERR) || (flags & EPOLLHUP) || (flags & EPOLLRDHUP)) {
        CloseConnection(fd);
        continue;
      }
      if (!(flags & EPOLLIN)) {
        continue;
      }

      std::shared_ptr<TcpConnection> connection;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) {
          continue;
        }
        connection = it->second;
      }

      bool peer_closed = false;
      while (true) {
        char chunk[8192];
        const ssize_t received = recv(fd, chunk, sizeof(chunk), 0);
        if (received > 0) {
          std::vector<std::string> frames;
          {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& buffer = read_buffers_[fd];
            buffer.append(chunk, static_cast<size_t>(received));
            std::string payload;
            while (TryDecodeFrame(buffer, payload)) {
              frames.push_back(payload);
            }
          }
          for (const auto& frame : frames) {
            workers_.Enqueue([this, connection, frame]() {
              try {
                on_message_(connection, frame);
              } catch (const std::exception& ex) {
                spdlog::error("message handler failed: {}", ex.what());
              }
              return 0;
            });
          }
          continue;
        }
        if (received == 0) {
          peer_closed = true;
          break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        peer_closed = true;
        break;
      }
      if (peer_closed) {
        CloseConnection(fd);
      }
    }
  }
}

}  // namespace dist_platform