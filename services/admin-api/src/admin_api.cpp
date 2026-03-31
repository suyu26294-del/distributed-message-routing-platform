#include "admin_api/admin_api.hpp"

#include "platform/common/models.hpp"

#include <sstream>

namespace admin_api {

namespace {

std::string json_escape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (const auto ch : input) {
        if (ch == '"') {
            out += "\\\"";
        } else {
            out += ch;
        }
    }
    return out;
}

}  // namespace

AdminApi::AdminApi(
    scheduler::SchedulerService& scheduler,
    message_service::MessageService& message_service,
    file_service::FileService& file_service)
    : scheduler_(scheduler), message_service_(message_service), file_service_(file_service) {}

std::string AdminApi::cluster_overview_json() const {
    std::ostringstream out;
    out << "{\"nodes\":[";
    const auto nodes = scheduler_.list_nodes();
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        if (index != 0) {
            out << ",";
        }
        out << "{"
            << "\"nodeId\":\"" << json_escape(node.node_id) << "\","
            << "\"service\":\"" << platform::common::to_string(node.service) << "\","
            << "\"load\":" << node.load.weighted() << ","
            << "\"lastHeartbeat\":\"" << platform::common::timestamp_string(node.last_heartbeat) << "\""
            << "}";
    }
    out << "]}";
    return out.str();
}

std::string AdminApi::route_stats_json() const {
    std::ostringstream out;
    out << "{\"ringSize\":" << scheduler_.ring().size()
        << ",\"schedulerLookups\":" << scheduler_.metrics().counter("scheduler_route_lookup")
        << "}";
    return out.str();
}

std::string AdminApi::message_stats_json() const {
    std::ostringstream out;
    out << "{\"offlineStored\":" << message_service_.metrics().counter("message_offline_stored")
        << ",\"onlineDelivered\":" << message_service_.metrics().counter("message_online_delivery")
        << ",\"deduplicated\":" << message_service_.metrics().counter("message_deduplicated")
        << ",\"unread\":[";
    const auto unread = message_service_.unread_counters();
    for (std::size_t index = 0; index < unread.size(); ++index) {
        const auto& counter = unread[index];
        if (index != 0) {
            out << ",";
        }
        out << "{\"userId\":\"" << json_escape(counter.user_id)
            << "\",\"conversationId\":\"" << json_escape(counter.conversation_id)
            << "\",\"count\":" << counter.unread << "}";
    }
    out << "]}";
    return out.str();
}

std::string AdminApi::file_stats_json() const {
    std::ostringstream out;
    out << "{\"uploads\":" << file_service_.metrics().counter("file_upload_init")
        << ",\"chunksUploaded\":" << file_service_.metrics().counter("file_chunk_upload")
        << ",\"files\":[";
    const auto files = file_service_.files();
    for (std::size_t index = 0; index < files.size(); ++index) {
        const auto& file = files[index];
        if (index != 0) {
            out << ",";
        }
        out << "{\"fileId\":\"" << json_escape(file.file_id)
            << "\",\"name\":\"" << json_escape(file.file_name)
            << "\",\"size\":" << file.file_size
            << ",\"available\":" << (file.available ? "true" : "false") << "}";
    }
    out << "]}";
    return out.str();
}

}  // namespace admin_api

