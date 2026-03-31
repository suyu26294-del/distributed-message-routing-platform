#include "platform/observability/metrics_registry.hpp"

namespace platform::observability {

void MetricsRegistry::increment(const std::string& name, std::uint64_t by) {
    counters_[name] += by;
}

void MetricsRegistry::set_gauge(const std::string& name, double value) {
    gauges_[name] = value;
}

std::uint64_t MetricsRegistry::counter(const std::string& name) const {
    if (const auto it = counters_.find(name); it != counters_.end()) {
        return it->second;
    }
    return 0;
}

double MetricsRegistry::gauge(const std::string& name) const {
    if (const auto it = gauges_.find(name); it != gauges_.end()) {
        return it->second;
    }
    return 0.0;
}

std::vector<std::string> MetricsRegistry::dump_lines() const {
    std::vector<std::string> lines;
    lines.reserve(counters_.size() + gauges_.size());
    for (const auto& [name, value] : counters_) {
        lines.push_back(name + "_total " + std::to_string(value));
    }
    for (const auto& [name, value] : gauges_) {
        lines.push_back(name + " " + std::to_string(value));
    }
    return lines;
}

}  // namespace platform::observability
