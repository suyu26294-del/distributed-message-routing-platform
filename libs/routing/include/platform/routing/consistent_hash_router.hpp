#pragma once

#include "platform/common/models.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace platform::routing {

class ConsistentHashRouter {
public:
    explicit ConsistentHashRouter(std::size_t virtual_nodes = 31);

    void upsert_node(const platform::common::NodeDescriptor& node);
    void remove_node(const std::string& node_id);
    [[nodiscard]] std::optional<platform::common::RouteDecision> resolve(
        platform::common::RouteTarget target,
        const std::string& route_key) const;
    [[nodiscard]] std::vector<platform::common::NodeDescriptor> nodes() const;
    [[nodiscard]] std::vector<std::pair<std::size_t, std::string>> ring() const;

private:
    [[nodiscard]] static std::size_t hash_value(const std::string& input);

    std::size_t virtual_nodes_;
    std::map<std::size_t, std::string> ring_;
    std::vector<platform::common::NodeDescriptor> nodes_;
};

}  // namespace platform::routing

