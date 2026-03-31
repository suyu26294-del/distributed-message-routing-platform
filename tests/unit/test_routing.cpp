#include "platform/common/models.hpp"
#include "platform/routing/consistent_hash_router.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "test failure: " << message << "\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    platform::routing::ConsistentHashRouter router(17);
    router.upsert_node({
        .node_id = "message-a",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6001,
        .load = {.cpu_percent = 70.0, .active_connections = 300, .queue_depth = 15},
    });
    router.upsert_node({
        .node_id = "message-b",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6002,
        .load = {.cpu_percent = 10.0, .active_connections = 30, .queue_depth = 1},
    });

    const auto first = router.resolve(platform::common::RouteTarget::Message, "user:1001");
    expect(first.has_value(), "route should exist");
    expect(first->node.has_value(), "route should include node");
    expect(first->node->node_id == "message-b", "lower load node should win");

    const auto before_remove = router.resolve(platform::common::RouteTarget::Message, "user:1001");
    router.remove_node("message-a");
    const auto after_remove = router.resolve(platform::common::RouteTarget::Message, "user:1001");

    expect(before_remove.has_value(), "route before removal should exist");
    expect(after_remove.has_value(), "route after removal should exist");
    expect(after_remove->node.has_value(), "route after removal should include node");
    expect(after_remove->node->node_id == "message-b", "remaining node should own route");

    std::cout << "unit_routing_test passed\n";
    return 0;
}

