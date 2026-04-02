#include "dist_platform/random_id.hpp"
#include "dist_platform/time_utils.hpp"

#include <atomic>
#include <sstream>

namespace dist_platform {
namespace {
std::atomic<uint64_t> counter{0};
}

std::string RandomId(const std::string& prefix) {
  std::ostringstream stream;
  stream << prefix << '-' << NowMillis() << '-' << counter.fetch_add(1, std::memory_order_relaxed);
  return stream.str();
}

}  // namespace dist_platform