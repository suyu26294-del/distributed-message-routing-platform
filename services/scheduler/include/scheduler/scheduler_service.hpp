#pragma once

#include "platform/common/models.hpp"
#include "platform/observability/metrics_registry.hpp"
#include "platform/routing/consistent_hash_router.hpp"

#include <optional>
#include <string>
#include <vector>

namespace scheduler {

class SchedulerService {
public:
    SchedulerService();

    void register_node(const platform::common::NodeDescriptor& node);
    void heartbeat(const std::string& node_id, const platform::common::LoadScore& load);
    [[nodiscard]] platform::common::RouteDecision resolve_route(
        platform::common::RouteTarget target,
        const std::string& route_key) const;
    [[nodiscard]] std::vector<platform::common::NodeDescriptor> list_nodes() const;
    [[nodiscard]] std::vector<std::pair<std::size_t, std::string>> ring() const;
    [[nodiscard]] const platform::observability::MetricsRegistry& metrics() const;

private:
    platform::routing::ConsistentHashRouter router_;
    mutable platform::observability::MetricsRegistry metrics_;
};

}  // namespace scheduler
