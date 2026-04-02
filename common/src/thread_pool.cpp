#include "dist_platform/thread_pool.hpp"

namespace dist_platform {

ThreadPool::ThreadPool(size_t thread_count) {
  threads_.reserve(thread_count);
  for (size_t i = 0; i < thread_count; ++i) {
    threads_.emplace_back([this]() {
      while (true) {
        std::function<void()> task;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
          if (stopping_ && tasks_.empty()) {
            return;
          }
          task = std::move(tasks_.front());
          tasks_.pop();
        }
        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto& thread : threads_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

}  // namespace dist_platform