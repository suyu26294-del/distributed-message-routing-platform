#include "dist_platform/time_utils.hpp"

#include <chrono>

namespace dist_platform {

int64_t NowMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace dist_platform