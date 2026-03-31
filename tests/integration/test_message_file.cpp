#include "file_service/file_service.hpp"
#include "gateway/gateway_service.hpp"
#include "message_service/message_service.hpp"
#include "platform/common/models.hpp"
#include "platform/storage/in_memory_store.hpp"
#include "scheduler/scheduler_service.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "integration failure: " << message << "\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    platform::storage::InMemoryStore store;
    scheduler::SchedulerService scheduler;
    message_service::MessageService message_service(store);
    file_service::FileService file_service(store);
    gateway::GatewayService gateway("gateway-a", scheduler, message_service, file_service);

    scheduler.register_node({
        .node_id = "message-a",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6001,
        .load = {.cpu_percent = 10.0, .active_connections = 12, .queue_depth = 1},
    });
    scheduler.register_node({
        .node_id = "file-a",
        .service = platform::common::ServiceKind::File,
        .host = "127.0.0.1",
        .port = 7001,
        .load = {.cpu_percent = 8.0, .active_connections = 6, .queue_depth = 0},
    });

    const platform::net::ConnectionContext alice {
        .connection_id = "conn-a",
        .user_id = "u1000",
        .remote_endpoint = "127.0.0.1:54000",
    };
    const platform::net::ConnectionContext bob {
        .connection_id = "conn-b",
        .user_id = "u1001",
        .remote_endpoint = "127.0.0.1:54001",
    };

    auto auth = gateway.handle_frame(
        alice, {.command = platform::common::CommandType::Auth, .request_id = 1, .user_id = "u1000", .body = "token"});
    expect(auth.ok, "alice auth should succeed");

    auto send = gateway.handle_frame(
        alice,
        {.command = platform::common::CommandType::SendMessage,
         .request_id = 2,
         .user_id = "u1000",
         .body = "u1001|offline hi"});
    expect(send.ok, "offline send should succeed");
    expect(message_service.unread_counters().size() == 1, "offline unread should be tracked");

    auth = gateway.handle_frame(
        bob, {.command = platform::common::CommandType::Auth, .request_id = 3, .user_id = "u1001", .body = "token"});
    expect(auth.ok, "bob auth should succeed");
    const auto offline = gateway.handle_frame(
        bob, {.command = platform::common::CommandType::PullOffline, .request_id = 4, .user_id = "u1001", .body = ""});
    expect(offline.body == "OFFLINE_COUNT=1", "bob should receive one offline message");

    const auto upload = gateway.handle_frame(
        alice, {.command = platform::common::CommandType::InitUpload, .request_id = 5, .user_id = "u1000", .body = "demo.bin"});
    expect(upload.ok, "upload init should succeed");
    const auto first_delimiter = upload.body.find('|');
    const auto second_delimiter = upload.body.find('|', first_delimiter + 1);
    const auto upload_id = upload.body.substr(0, first_delimiter);
    const auto file_id = upload.body.substr(first_delimiter + 1, second_delimiter - first_delimiter - 1);

    const auto chunk0 = gateway.handle_frame(
        alice,
        {.command = platform::common::CommandType::UploadChunk,
         .request_id = 6,
         .user_id = "u1000",
         .body = upload_id + "|0|tiny"});
    expect(chunk0.ok, "chunk upload should succeed");

    const auto resume = file_service.query_resume_state(upload_id);
    expect(!resume.missing_chunks.empty(), "resume should report missing chunks");
    expect(resume.next_chunk == 1, "next missing chunk should be 1");

    const auto bytes = file_service.download_chunk(file_id, 0);
    expect(bytes.size() == 4, "hot chunk should be readable");

    std::cout << "integration_message_file_test passed\n";
    return 0;
}
