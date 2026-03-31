#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace platform::observability {

class MetricsRegistry {
public:
    void increment(const std::string& name, std::uint64_t by = 1);
    void set_gauge(const std::string& name, double value);
    [[nodiscard]] std::uint64_t counter(const std::string& name) const;
    [[nodiscard]] double gauge(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> dump_lines() const;

private:
    std::unordered_map<std::string, std::uint64_t> counters_;
    std::unordered_map<std::string, double> gauges_;
};

}  // namespace platform::observability

