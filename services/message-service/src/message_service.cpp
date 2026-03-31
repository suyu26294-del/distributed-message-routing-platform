#include "message_service/message_service.hpp"

namespace message_service {

MessageService::MessageService(platform::storage::InMemoryStore& store) : store_(store) {}

void MessageService::mark_online(const std::string& user_id, const std::string& gateway_node_id) {
    store_.put_session(user_id, gateway_node_id);
    metrics_.increment("message_session_online");
}

DeliveryResult MessageService::deliver(const platform::common::MessageEnvelope& message) {
    if (!store_.mark_message_seen(message.message_id)) {
        metrics_.increment("message_deduplicated");
        return {.accepted = true, .delivered_online = false, .deduplicated = true, .detail = "duplicate"};
    }

    store_.save_message(message);
    const auto recipient_session = store_.get_session(message.recipient_id);
    if (recipient_session.has_value()) {
        metrics_.increment("message_online_delivery");
        return {.accepted = true, .delivered_online = true, .deduplicated = false, .detail = *recipient_session};
    }

    store_.save_offline_message(message);
    store_.increment_unread(message.recipient_id, message.conversation_id);
    metrics_.increment("message_offline_stored");
    return {.accepted = true, .delivered_online = false, .deduplicated = false, .detail = "stored-offline"};
}

std::vector<platform::common::MessageEnvelope> MessageService::pull_offline(const std::string& user_id) {
    auto values = store_.pull_offline(user_id);
    for (const auto& message : values) {
        store_.clear_unread(user_id, message.conversation_id);
    }
    metrics_.increment("message_offline_pull", values.size());
    return values;
}

void MessageService::ack_delivered(const std::string& user_id, const std::string& conversation_id) {
    store_.clear_unread(user_id, conversation_id);
    metrics_.increment("message_ack");
}

const platform::observability::MetricsRegistry& MessageService::metrics() const {
    return metrics_;
}

std::vector<platform::common::UnreadCounter> MessageService::unread_counters() const {
    return store_.unread_counters();
}

}  // namespace message_service

