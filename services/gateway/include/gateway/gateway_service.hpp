#pragma once

#include "file_service/file_service.hpp"
#include "message_service/message_service.hpp"
#include "platform/common/models.hpp"
#include "platform/net/reactor_stub.hpp"
#include "platform/observability/metrics_registry.hpp"
#include "platform/protocol/frame_codec.hpp"
#include "scheduler/scheduler_service.hpp"

#include <string>
#include <unordered_map>

namespace gateway {

struct GatewayResponse {
    bool ok {false};
    std::string body;
};

class GatewayService {
public:
    GatewayService(
        std::string gateway_node_id,
        scheduler::SchedulerService& scheduler,
        message_service::MessageService& message_service,
        file_service::FileService& file_service);

    GatewayResponse handle_frame(const platform::net::ConnectionContext& context, const platform::protocol::Frame& frame);
    [[nodiscard]] const platform::observability::MetricsRegistry& metrics() const;

private:
    [[nodiscard]] bool is_authenticated(const std::string& user_id) const;
    [[nodiscard]] platform::common::RouteDecision cached_route_or_resolve(
        platform::common::RouteTarget target,
        const std::string& route_key);

    std::string gateway_node_id_;
    scheduler::SchedulerService& scheduler_;
    message_service::MessageService& message_service_;
    file_service::FileService& file_service_;
    platform::observability::MetricsRegistry metrics_;
    std::unordered_map<std::string, bool> authenticated_;
    std::unordered_map<std::string, platform::common::RouteDecision> route_cache_;
};

}  // namespace gateway

