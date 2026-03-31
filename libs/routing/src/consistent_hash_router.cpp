#include "platform/routing/consistent_hash_router.hpp"

#include <algorithm>
#include <functional>

namespace platform::routing {

namespace {

bool same_target(platform::common::ServiceKind service, platform::common::RouteTarget target) {
    if (target == platform::common::RouteTarget::Message) {
        return service == platform::common::ServiceKind::Message;
    }
    return service == platform::common::ServiceKind::File;
}

}  // namespace

ConsistentHashRouter::ConsistentHashRouter(std::size_t virtual_nodes)
    : virtual_nodes_(virtual_nodes) {}

void ConsistentHashRouter::upsert_node(const platform::common::NodeDescriptor& node) {
    remove_node(node.node_id);
    nodes_.push_back(node);
    for (std::size_t replica = 0; replica < virtual_nodes_; ++replica) {
        ring_.emplace(hash_value(node.node_id + "#" + std::to_string(replica)), node.node_id);
    }
}

void ConsistentHashRouter::remove_node(const std::string& node_id) {
    nodes_.erase(
        std::remove_if(nodes_.begin(), nodes_.end(),
                       [&](const auto& item) { return item.node_id == node_id; }),
        nodes_.end());

    for (auto it = ring_.begin(); it != ring_.end();) {
        if (it->second == node_id) {
            it = ring_.erase(it);
        } else {
            ++it;
        }
    }
}

std::optional<platform::common::RouteDecision> ConsistentHashRouter::resolve(
    platform::common::RouteTarget target,
    const std::string& route_key) const {
    std::vector<platform::common::NodeDescriptor> candidates;
    candidates.reserve(nodes_.size());
    for (const auto& node : nodes_) {
        if (node.healthy && same_target(node.service, target)) {
            candidates.push_back(node);
        }
    }

    if (candidates.empty()) {
        return platform::common::RouteDecision {
            route_key,
            target,
            std::nullopt,
            "no healthy nodes",
        };
    }

    const auto hash = hash_value(route_key);
    auto it = ring_.lower_bound(hash);
    if (it == ring_.end()) {
        it = ring_.begin();
    }

    std::optional<platform::common::NodeDescriptor> best;
    double best_score = 0.0;
    std::size_t checked = 0;
    auto cursor = it;
    while (checked < ring_.size()) {
        const auto found = std::find_if(candidates.begin(), candidates.end(), [&](const auto& node) {
            return node.node_id == cursor->second;
        });
        if (found != candidates.end()) {
            const double score = found->load.weighted();
            if (!best.has_value() || score < best_score) {
                best = *found;
                best_score = score;
            }
            if (best_score < 20.0) {
                break;
            }
        }
        ++checked;
        ++cursor;
        if (cursor == ring_.end()) {
            cursor = ring_.begin();
        }
    }

    return platform::common::RouteDecision {
        route_key,
        target,
        best,
        best.has_value() ? "consistent-hash-with-load" : "no matching node on ring",
    };
}

std::vector<platform::common::NodeDescriptor> ConsistentHashRouter::nodes() const {
    return nodes_;
}

std::vector<std::pair<std::size_t, std::string>> ConsistentHashRouter::ring() const {
    return std::vector<std::pair<std::size_t, std::string>>(ring_.begin(), ring_.end());
}

std::size_t ConsistentHashRouter::hash_value(const std::string& input) {
    return std::hash<std::string> {}(input);
}

}  // namespace platform::routing
