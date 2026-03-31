#include "scheduler/scheduler_service.hpp"

namespace scheduler {

SchedulerService::SchedulerService() = default;

void SchedulerService::register_node(const platform::common::NodeDescriptor& node) {
    router_.upsert_node(node);
    metrics_.increment("scheduler_node_register");
    metrics_.set_gauge("scheduler_nodes", static_cast<double>(router_.nodes().size()));
}

void SchedulerService::heartbeat(const std::string& node_id, const platform::common::LoadScore& load) {
    for (auto node : router_.nodes()) {
        if (node.node_id == node_id) {
            node.load = load;
            node.last_heartbeat = platform::common::Clock::now();
            node.healthy = true;
            router_.upsert_node(node);
            metrics_.increment("scheduler_heartbeat");
            return;
        }
    }
}

platform::common::RouteDecision SchedulerService::resolve_route(
    platform::common::RouteTarget target,
    const std::string& route_key) const {
    metrics_.increment("scheduler_route_lookup");
    auto result = router_.resolve(target, route_key);
    if (result.has_value()) {
        return *result;
    }
    return platform::common::RouteDecision {route_key, target, std::nullopt, "no-route"};
}

std::vector<platform::common::NodeDescriptor> SchedulerService::list_nodes() const {
    return router_.nodes();
}

std::vector<std::pair<std::size_t, std::string>> SchedulerService::ring() const {
    return router_.ring();
}

const platform::observability::MetricsRegistry& SchedulerService::metrics() const {
    return metrics_;
}

}  // namespace scheduler

