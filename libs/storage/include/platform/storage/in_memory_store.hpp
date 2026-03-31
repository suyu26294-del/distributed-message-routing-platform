#pragma once

#include "platform/common/models.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace platform::storage {

class InMemoryStore {
public:
    void put_session(const std::string& user_id, const std::string& node_id);
    [[nodiscard]] std::optional<std::string> get_session(const std::string& user_id) const;

    bool mark_message_seen(const std::string& message_id);
    [[nodiscard]] bool is_message_seen(const std::string& message_id) const;

    void save_message(const platform::common::MessageEnvelope& message);
    void save_offline_message(const platform::common::MessageEnvelope& message);
    [[nodiscard]] std::vector<platform::common::MessageEnvelope> pull_offline(const std::string& user_id);

    void increment_unread(const std::string& user_id, const std::string& conversation_id);
    void clear_unread(const std::string& user_id, const std::string& conversation_id);
    [[nodiscard]] std::vector<platform::common::UnreadCounter> unread_counters() const;

    void save_upload_session(const platform::common::UploadSession& session);
    [[nodiscard]] std::optional<platform::common::UploadSession> get_upload_session(
        const std::string& upload_id) const;
    void record_uploaded_chunk(const std::string& upload_id, std::uint32_t chunk_no);

    void save_file_meta(const platform::common::FileMeta& meta);
    [[nodiscard]] std::optional<platform::common::FileMeta> get_file_meta(const std::string& file_id) const;
    [[nodiscard]] std::vector<platform::common::FileMeta> list_files() const;

    void save_chunk_meta(const platform::common::FileChunkMeta& chunk);
    [[nodiscard]] std::vector<platform::common::FileChunkMeta> list_chunks(const std::string& file_id) const;

    void put_hot_chunk(const std::string& file_id, std::uint32_t chunk_no, std::vector<std::uint8_t> bytes);
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> get_hot_chunk(
        const std::string& file_id,
        std::uint32_t chunk_no) const;

private:
    std::unordered_map<std::string, std::string> sessions_;
    std::unordered_set<std::string> seen_message_ids_;
    std::vector<platform::common::MessageEnvelope> persisted_messages_;
    std::unordered_map<std::string, std::vector<platform::common::MessageEnvelope>> offline_messages_;
    std::unordered_map<std::string, platform::common::UnreadCounter> unread_;
    std::unordered_map<std::string, platform::common::UploadSession> upload_sessions_;
    std::unordered_map<std::string, platform::common::FileMeta> files_;
    std::unordered_map<std::string, std::vector<platform::common::FileChunkMeta>> chunks_;
    std::unordered_map<std::string, std::vector<std::uint8_t>> hot_chunks_;
};

}  // namespace platform::storage

