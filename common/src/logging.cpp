#include "dist_platform/logging.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace dist_platform {

void InitLogging(const std::string& service_name) {
  auto logger = spdlog::stdout_color_mt(service_name);
  spdlog::set_default_logger(logger);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
  spdlog::set_level(spdlog::level::info);
}

}  // namespace dist_platform