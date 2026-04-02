#pragma once

#include "dist_platform/thread_pool.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace dist_platform {

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
 public:
  explicit TcpConnection(int fd);
  ~TcpConnection();

  int Fd() const;
  void SendFrame(const std::string& payload);
  void MarkClosed();
  bool IsClosed() const;
  void SetTag(const std::string& key, const std::string& value);
  std::string GetTag(const std::string& key) const;

 private:
  int fd_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> tags_;
  std::atomic<bool> closed_;
};

class TcpFrameServer {
 public:
  using MessageHandler = std::function<void(const std::shared_ptr<TcpConnection>&, const std::string&)>;
  using DisconnectHandler = std::function<void(const std::shared_ptr<TcpConnection>&)>;

  TcpFrameServer(std::string host,
                 uint16_t port,
                 size_t worker_count,
                 MessageHandler on_message,
                 DisconnectHandler on_disconnect = {});
  ~TcpFrameServer();

  void Start();
  void Stop();
  size_t ConnectionCount() const;

 private:
  void EventLoop();
  void AcceptLoop();
  void CloseConnection(int fd);

  std::string host_;
  uint16_t port_;
  int listen_fd_;
  int epoll_fd_;
  MessageHandler on_message_;
  DisconnectHandler on_disconnect_;
  ThreadPool workers_;
  std::thread loop_thread_;
  std::atomic<bool> running_;
  mutable std::mutex mutex_;
  std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;
  std::unordered_map<int, std::string> read_buffers_;
};

}  // namespace dist_platform