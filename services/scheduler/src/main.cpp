#include "scheduler/scheduler_service.hpp"

#include <iostream>

int main() {
    scheduler::SchedulerService service;
    service.register_node({
        .node_id = "message-a",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6001,
        .load = {.cpu_percent = 12.0, .active_connections = 100, .queue_depth = 4},
    });

    const auto route = service.resolve_route(platform::common::RouteTarget::Message, "user:1001");
    std::cout << "scheduler ready; route reason=" << route.reason << "\n";
    return 0;
}

