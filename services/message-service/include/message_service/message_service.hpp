#pragma once

#include "platform/common/models.hpp"
#include "platform/observability/metrics_registry.hpp"
#include "platform/storage/in_memory_store.hpp"

#include <string>
#include <vector>

namespace message_service {

struct DeliveryResult {
    bool accepted {false};
    bool delivered_online {false};
    bool deduplicated {false};
    std::string detail;
};

class MessageService {
public:
    explicit MessageService(platform::storage::InMemoryStore& store);

    void mark_online(const std::string& user_id, const std::string& gateway_node_id);
    DeliveryResult deliver(const platform::common::MessageEnvelope& message);
    std::vector<platform::common::MessageEnvelope> pull_offline(const std::string& user_id);
    void ack_delivered(const std::string& user_id, const std::string& conversation_id);

    [[nodiscard]] const platform::observability::MetricsRegistry& metrics() const;
    [[nodiscard]] std::vector<platform::common::UnreadCounter> unread_counters() const;

private:
    platform::storage::InMemoryStore& store_;
    platform::observability::MetricsRegistry metrics_;
};

}  // namespace message_service

