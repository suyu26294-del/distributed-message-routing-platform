#include "message_service/message_service.hpp"

#include <iostream>

int main() {
    platform::storage::InMemoryStore store;
    message_service::MessageService service(store);
    service.mark_online("u1001", "gateway-a");
    const auto result = service.deliver({
        .message_id = "m-1",
        .conversation_id = "c-1",
        .sender_id = "u1000",
        .recipient_id = "u1001",
        .body = "hello",
    });
    std::cout << "message-service ready; delivered_online=" << result.delivered_online << "\n";
    return 0;
}

