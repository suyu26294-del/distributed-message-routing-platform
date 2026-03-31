#include "platform/common/models.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace platform::common {

double LoadScore::weighted() const {
    return cpu_percent * 0.5 + static_cast<double>(active_connections) * 0.3 +
           static_cast<double>(queue_depth) * 0.2;
}

std::string to_string(ServiceKind kind) {
    switch (kind) {
        case ServiceKind::Gateway:
            return "gateway";
        case ServiceKind::Scheduler:
            return "scheduler";
        case ServiceKind::Message:
            return "message-service";
        case ServiceKind::File:
            return "file-service";
        case ServiceKind::Admin:
            return "admin-api";
    }
    return "unknown";
}

std::string to_string(RouteTarget target) {
    switch (target) {
        case RouteTarget::Message:
            return "message";
        case RouteTarget::File:
            return "file";
    }
    return "unknown";
}

std::string timestamp_string(TimePoint value) {
    const auto time = Clock::to_time_t(value);
    std::tm tm {};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

}  // namespace platform::common

