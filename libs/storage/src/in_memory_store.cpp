#include "platform/storage/in_memory_store.hpp"

#include <algorithm>

namespace platform::storage {

void InMemoryStore::put_session(const std::string& user_id, const std::string& node_id) {
    sessions_[user_id] = node_id;
}

std::optional<std::string> InMemoryStore::get_session(const std::string& user_id) const {
    if (const auto it = sessions_.find(user_id); it != sessions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool InMemoryStore::mark_message_seen(const std::string& message_id) {
    return seen_message_ids_.insert(message_id).second;
}

bool InMemoryStore::is_message_seen(const std::string& message_id) const {
    return seen_message_ids_.contains(message_id);
}

void InMemoryStore::save_message(const platform::common::MessageEnvelope& message) {
    persisted_messages_.push_back(message);
}

void InMemoryStore::save_offline_message(const platform::common::MessageEnvelope& message) {
    offline_messages_[message.recipient_id].push_back(message);
}

std::vector<platform::common::MessageEnvelope> InMemoryStore::pull_offline(const std::string& user_id) {
    auto found = offline_messages_.find(user_id);
    if (found == offline_messages_.end()) {
        return {};
    }
    auto messages = found->second;
    offline_messages_.erase(found);
    return messages;
}

void InMemoryStore::increment_unread(const std::string& user_id, const std::string& conversation_id) {
    const auto key = user_id + ":" + conversation_id;
    auto& value = unread_[key];
    value.user_id = user_id;
    value.conversation_id = conversation_id;
    ++value.unread;
}

void InMemoryStore::clear_unread(const std::string& user_id, const std::string& conversation_id) {
    const auto key = user_id + ":" + conversation_id;
    if (auto it = unread_.find(key); it != unread_.end()) {
        it->second.unread = 0;
    }
}

std::vector<platform::common::UnreadCounter> InMemoryStore::unread_counters() const {
    std::vector<platform::common::UnreadCounter> values;
    values.reserve(unread_.size());
    for (const auto& [_, counter] : unread_) {
        values.push_back(counter);
    }
    return values;
}

void InMemoryStore::save_upload_session(const platform::common::UploadSession& session) {
    upload_sessions_[session.upload_id] = session;
}

std::optional<platform::common::UploadSession> InMemoryStore::get_upload_session(const std::string& upload_id) const {
    if (const auto it = upload_sessions_.find(upload_id); it != upload_sessions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void InMemoryStore::record_uploaded_chunk(const std::string& upload_id, std::uint32_t chunk_no) {
    auto& session = upload_sessions_[upload_id];
    if (std::find(session.uploaded_chunks.begin(), session.uploaded_chunks.end(), chunk_no) ==
        session.uploaded_chunks.end()) {
        session.uploaded_chunks.push_back(chunk_no);
    }
}

void InMemoryStore::save_file_meta(const platform::common::FileMeta& meta) {
    files_[meta.file_id] = meta;
}

std::optional<platform::common::FileMeta> InMemoryStore::get_file_meta(const std::string& file_id) const {
    if (const auto it = files_.find(file_id); it != files_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<platform::common::FileMeta> InMemoryStore::list_files() const {
    std::vector<platform::common::FileMeta> values;
    values.reserve(files_.size());
    for (const auto& [_, file] : files_) {
        values.push_back(file);
    }
    return values;
}

void InMemoryStore::save_chunk_meta(const platform::common::FileChunkMeta& chunk) {
    chunks_[chunk.file_id].push_back(chunk);
}

std::vector<platform::common::FileChunkMeta> InMemoryStore::list_chunks(const std::string& file_id) const {
    if (const auto it = chunks_.find(file_id); it != chunks_.end()) {
        return it->second;
    }
    return {};
}

void InMemoryStore::put_hot_chunk(const std::string& file_id, std::uint32_t chunk_no, std::vector<std::uint8_t> bytes) {
    hot_chunks_[file_id + ":" + std::to_string(chunk_no)] = std::move(bytes);
}

std::optional<std::vector<std::uint8_t>> InMemoryStore::get_hot_chunk(
    const std::string& file_id,
    std::uint32_t chunk_no) const {
    if (const auto it = hot_chunks_.find(file_id + ":" + std::to_string(chunk_no)); it != hot_chunks_.end()) {
        return it->second;
    }
    return std::nullopt;
}

}  // namespace platform::storage

