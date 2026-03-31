#include "admin_api/admin_api.hpp"
#include "file_service/file_service.hpp"
#include "gateway/gateway_service.hpp"
#include "message_service/message_service.hpp"
#include "platform/common/models.hpp"
#include "platform/net/reactor_stub.hpp"
#include "scheduler/scheduler_service.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

void print_section(const std::string& title) {
    std::cout << "\n== " << title << " ==\n";
}

}  // namespace

int main() {
    platform::storage::InMemoryStore store;
    scheduler::SchedulerService scheduler;
    message_service::MessageService message_service(store);
    file_service::FileService file_service(store);
    gateway::GatewayService gateway("gateway-a", scheduler, message_service, file_service);
    admin_api::AdminApi admin(scheduler, message_service, file_service);

    scheduler.register_node({
        .node_id = "message-a",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6001,
        .load = {.cpu_percent = 15.0, .active_connections = 120, .queue_depth = 3},
    });
    scheduler.register_node({
        .node_id = "message-b",
        .service = platform::common::ServiceKind::Message,
        .host = "127.0.0.1",
        .port = 6002,
        .load = {.cpu_percent = 9.0, .active_connections = 80, .queue_depth = 2},
    });
    scheduler.register_node({
        .node_id = "file-a",
        .service = platform::common::ServiceKind::File,
        .host = "127.0.0.1",
        .port = 7001,
        .load = {.cpu_percent = 11.0, .active_connections = 40, .queue_depth = 1},
    });

    const platform::net::ConnectionContext alice_context {
        .connection_id = "conn-alice",
        .user_id = "u1000",
        .remote_endpoint = "127.0.0.1:54000",
    };
    const platform::net::ConnectionContext bob_context {
        .connection_id = "conn-bob",
        .user_id = "u1001",
        .remote_endpoint = "127.0.0.1:54001",
    };

    print_section("auth");
    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::Auth, .request_id = 1, .user_id = "u1000", .body = "token-alice"})
                     .body
              << "\n";

    print_section("offline message flow");
    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::SendMessage,
                      .request_id = 2,
                      .user_id = "u1000",
                      .body = "u1001|hello while offline"})
                     .body
              << "\n";

    std::cout << gateway.handle_frame(
                     bob_context,
                     {.command = platform::common::CommandType::Auth, .request_id = 3, .user_id = "u1001", .body = "token-bob"})
                     .body
              << "\n";
    std::cout << gateway.handle_frame(
                     bob_context,
                     {.command = platform::common::CommandType::PullOffline, .request_id = 4, .user_id = "u1001", .body = ""})
                     .body
              << "\n";

    print_section("file resume flow");
    const auto upload_response = gateway.handle_frame(
        alice_context,
        {.command = platform::common::CommandType::InitUpload, .request_id = 5, .user_id = "u1000", .body = "demo-video.bin"});
    std::cout << upload_response.body << "\n";

    const auto first_delimiter = upload_response.body.find('|');
    const auto second_delimiter = upload_response.body.find('|', first_delimiter + 1);
    const auto upload_id = upload_response.body.substr(0, first_delimiter);
    const auto file_id = upload_response.body.substr(first_delimiter + 1, second_delimiter - first_delimiter - 1);

    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::UploadChunk,
                      .request_id = 6,
                      .user_id = "u1000",
                      .body = upload_id + "|0|chunk-zero"})
                     .body
              << "\n";
    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::ResumeUpload,
                      .request_id = 7,
                      .user_id = "u1000",
                      .body = upload_id})
                     .body
              << "\n";
    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::UploadChunk,
                      .request_id = 8,
                      .user_id = "u1000",
                      .body = upload_id + "|1|chunk-one"})
                     .body
              << "\n";
    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::CompleteUpload,
                      .request_id = 9,
                      .user_id = "u1000",
                      .body = upload_id})
                     .body
              << "\n";
    std::cout << gateway.handle_frame(
                     alice_context,
                     {.command = platform::common::CommandType::DownloadChunk,
                      .request_id = 10,
                      .user_id = "u1000",
                      .body = file_id + "|0"})
                     .body
              << "\n";

    print_section("admin snapshots");
    std::cout << admin.cluster_overview_json() << "\n";
    std::cout << admin.route_stats_json() << "\n";
    std::cout << admin.message_stats_json() << "\n";
    std::cout << admin.file_stats_json() << "\n";

    return 0;
}
