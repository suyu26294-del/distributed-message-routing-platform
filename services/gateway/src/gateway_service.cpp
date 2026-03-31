#include "gateway/gateway_service.hpp"

#include <sstream>

namespace gateway {

GatewayService::GatewayService(
    std::string gateway_node_id,
    scheduler::SchedulerService& scheduler,
    message_service::MessageService& message_service,
    file_service::FileService& file_service)
    : gateway_node_id_(std::move(gateway_node_id)),
      scheduler_(scheduler),
      message_service_(message_service),
      file_service_(file_service) {}

GatewayResponse GatewayService::handle_frame(
    const platform::net::ConnectionContext& context,
    const platform::protocol::Frame& frame) {
    metrics_.increment("gateway_frames_total");
    switch (frame.command) {
        case platform::common::CommandType::Auth:
            authenticated_[frame.user_id] = true;
            message_service_.mark_online(frame.user_id, gateway_node_id_);
            return {.ok = true, .body = "AUTH_OK"};
        case platform::common::CommandType::SendMessage: {
            if (!is_authenticated(frame.user_id)) {
                return {.ok = false, .body = "AUTH_REQUIRED"};
            }
            const auto delimiter = frame.body.find('|');
            const auto recipient = frame.body.substr(0, delimiter);
            const auto text = delimiter == std::string::npos ? "" : frame.body.substr(delimiter + 1);
            const auto route = cached_route_or_resolve(platform::common::RouteTarget::Message, recipient);
            const auto result = message_service_.deliver({
                .message_id = "msg-" + std::to_string(frame.request_id),
                .conversation_id = "conv-" + recipient,
                .sender_id = frame.user_id,
                .recipient_id = recipient,
                .body = text,
            });
            std::ostringstream out;
            out << (result.delivered_online ? "DELIVERED" : "STORED")
                << "|route=" << (route.node.has_value() ? route.node->node_id : "none");
            return {.ok = result.accepted, .body = out.str()};
        }
        case platform::common::CommandType::PullOffline: {
            const auto messages = message_service_.pull_offline(frame.user_id);
            return {.ok = true, .body = "OFFLINE_COUNT=" + std::to_string(messages.size())};
        }
        case platform::common::CommandType::InitUpload: {
            const auto session = file_service_.init_upload(frame.user_id, frame.body, 8 * 1024 * 1024);
            const auto route = cached_route_or_resolve(platform::common::RouteTarget::File, session.file_id);
            const auto route_node = route.node.has_value() ? route.node->node_id : "none";
            return {.ok = true, .body = session.upload_id + "|" + session.file_id + "|route=" + route_node};
        }
        case platform::common::CommandType::UploadChunk: {
            const auto first = frame.body.find('|');
            const auto second = frame.body.find('|', first + 1);
            const auto upload_id = frame.body.substr(0, first);
            const auto chunk_no = static_cast<std::uint32_t>(std::stoul(frame.body.substr(first + 1, second - first - 1)));
            const auto payload = frame.body.substr(second + 1);
            std::vector<std::uint8_t> bytes(payload.begin(), payload.end());
            const auto meta = file_service_.upload_chunk({
                .upload_id = upload_id,
                .chunk_no = chunk_no,
                .bytes = bytes,
                .node_id = "file-a",
            });
            return {.ok = true, .body = meta.file_id + "|chunk=" + std::to_string(meta.chunk_no)};
        }
        case platform::common::CommandType::CompleteUpload: {
            const auto meta = file_service_.complete_upload(frame.body);
            return {.ok = true, .body = meta.file_id + "|complete"};
        }
        case platform::common::CommandType::ResumeUpload: {
            const auto token = file_service_.query_resume_state(frame.body);
            return {.ok = true, .body = "NEXT=" + std::to_string(token.next_chunk)};
        }
        case platform::common::CommandType::DownloadChunk: {
            const auto split = frame.body.find('|');
            const auto file_id = frame.body.substr(0, split);
            const auto chunk_no = static_cast<std::uint32_t>(std::stoul(frame.body.substr(split + 1)));
            const auto bytes = file_service_.download_chunk(file_id, chunk_no);
            return {.ok = !bytes.empty(), .body = "BYTES=" + std::to_string(bytes.size())};
        }
        case platform::common::CommandType::Ack:
            message_service_.ack_delivered(frame.user_id, frame.body);
            return {.ok = true, .body = "ACK_OK"};
        case platform::common::CommandType::QueryFile:
            return {.ok = true, .body = "FILES=" + std::to_string(file_service_.files().size())};
    }
    return {.ok = false, .body = "UNSUPPORTED"};
}

const platform::observability::MetricsRegistry& GatewayService::metrics() const {
    return metrics_;
}

bool GatewayService::is_authenticated(const std::string& user_id) const {
    if (const auto it = authenticated_.find(user_id); it != authenticated_.end()) {
        return it->second;
    }
    return false;
}

platform::common::RouteDecision GatewayService::cached_route_or_resolve(
    platform::common::RouteTarget target,
    const std::string& route_key) {
    const auto cache_key = platform::common::to_string(target) + ":" + route_key;
    if (const auto it = route_cache_.find(cache_key); it != route_cache_.end()) {
        metrics_.increment("gateway_route_cache_hit");
        return it->second;
    }

    auto route = scheduler_.resolve_route(target, route_key);
    route_cache_[cache_key] = route;
    metrics_.increment("gateway_route_cache_miss");
    return route;
}

}  // namespace gateway
