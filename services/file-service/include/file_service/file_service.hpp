#pragma once

#include "platform/common/models.hpp"
#include "platform/observability/metrics_registry.hpp"
#include "platform/storage/in_memory_store.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace file_service {

struct UploadChunkRequest {
    std::string upload_id;
    std::uint32_t chunk_no {0};
    std::vector<std::uint8_t> bytes;
    std::string node_id;
};

class FileService {
public:
    explicit FileService(platform::storage::InMemoryStore& store);

    platform::common::UploadSession init_upload(
        const std::string& owner_id,
        const std::string& file_name,
        std::uint64_t file_size,
        std::uint32_t chunk_size = 4U * 1024U * 1024U);
    platform::common::FileChunkMeta upload_chunk(const UploadChunkRequest& request);
    platform::common::FileMeta complete_upload(const std::string& upload_id);
    platform::common::ResumeToken query_resume_state(const std::string& upload_id) const;
    std::vector<std::uint8_t> download_chunk(const std::string& file_id, std::uint32_t chunk_no) const;

    [[nodiscard]] std::vector<platform::common::FileMeta> files() const;
    [[nodiscard]] std::vector<platform::common::FileChunkMeta> chunks(const std::string& file_id) const;
    [[nodiscard]] const platform::observability::MetricsRegistry& metrics() const;

private:
    [[nodiscard]] static std::string checksum(const std::vector<std::uint8_t>& bytes);
    [[nodiscard]] static std::uint32_t total_chunks(std::uint64_t file_size, std::uint32_t chunk_size);

    platform::storage::InMemoryStore& store_;
    platform::observability::MetricsRegistry metrics_;
    std::unordered_map<std::string, std::unordered_map<std::uint32_t, std::vector<std::uint8_t>>> chunk_payloads_;
    std::uint64_t next_upload_id_ {1};
    std::uint64_t next_file_id_ {1};
};

}  // namespace file_service

