#include "gateway/gateway_service.hpp"

#include <iostream>

int main() {
    platform::storage::InMemoryStore store;
    scheduler::SchedulerService scheduler;
    message_service::MessageService message_service(store);
    file_service::FileService file_service(store);

    scheduler.register_node({
        .node_id = "message-a",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6001,
        .load = {.cpu_percent = 12.0, .active_connections = 10, .queue_depth = 1},
    });
    scheduler.register_node({
        .node_id = "file-a",
        .service = platform::common::ServiceKind::File,
        .host = "127.0.0.1",
        .port = 7001,
        .load = {.cpu_percent = 18.0, .active_connections = 2, .queue_depth = 0},
    });

    gateway::GatewayService gateway("gateway-a", scheduler, message_service, file_service);
    const auto response = gateway.handle_frame(
        {.connection_id = "conn-1", .user_id = "u1000", .remote_endpoint = "127.0.0.1:54000"},
        {.command = platform::common::CommandType::Auth, .request_id = 1, .user_id = "u1000", .body = "token"});
    std::cout << "gateway ready; response=" << response.body << "\n";
    return 0;
}

