#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace dist_platform {

class ThreadPool {
 public:
  explicit ThreadPool(size_t thread_count);
  ~ThreadPool();

  template <typename Func>
  auto Enqueue(Func&& func) -> std::future<decltype(func())> {
    using ResultType = decltype(func());
    auto task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<Func>(func));
    std::future<ResultType> future = task->get_future();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push([task]() { (*task)(); });
    }
    cv_.notify_one();
    return future;
  }

 private:
  std::vector<std::thread> threads_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stopping_ = false;
};

}  // namespace dist_platform