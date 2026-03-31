#include "file_service/file_service.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace file_service {

FileService::FileService(platform::storage::InMemoryStore& store) : store_(store) {}

platform::common::UploadSession FileService::init_upload(
    const std::string& owner_id,
    const std::string& file_name,
    std::uint64_t file_size,
    std::uint32_t chunk_size) {
    platform::common::UploadSession session {
        .upload_id = "upload-" + std::to_string(next_upload_id_++),
        .file_id = "file-" + std::to_string(next_file_id_++),
        .owner_id = owner_id,
        .file_name = file_name,
        .file_size = file_size,
        .chunk_size = chunk_size,
    };
    store_.save_upload_session(session);
    metrics_.increment("file_upload_init");
    return session;
}

platform::common::FileChunkMeta FileService::upload_chunk(const UploadChunkRequest& request) {
    const auto session = store_.get_upload_session(request.upload_id);
    if (!session.has_value()) {
        throw std::runtime_error("unknown upload session");
    }

    const auto offset = static_cast<std::uint64_t>(request.chunk_no) * session->chunk_size;
    platform::common::FileChunkMeta meta {
        .file_id = session->file_id,
        .chunk_no = request.chunk_no,
        .offset = offset,
        .size = static_cast<std::uint32_t>(request.bytes.size()),
        .checksum = checksum(request.bytes),
        .node_id = request.node_id,
    };
    store_.save_chunk_meta(meta);
    store_.record_uploaded_chunk(request.upload_id, request.chunk_no);
    if (request.bytes.size() <= 256 * 1024) {
        store_.put_hot_chunk(session->file_id, request.chunk_no, request.bytes);
    }
    chunk_payloads_[session->file_id][request.chunk_no] = request.bytes;
    metrics_.increment("file_chunk_upload");
    return meta;
}

platform::common::FileMeta FileService::complete_upload(const std::string& upload_id) {
    auto session = store_.get_upload_session(upload_id);
    if (!session.has_value()) {
        throw std::runtime_error("unknown upload session");
    }

    auto completed = *session;
    completed.completed = true;
    store_.save_upload_session(completed);

    platform::common::FileMeta meta {
        .file_id = completed.file_id,
        .owner_id = completed.owner_id,
        .file_name = completed.file_name,
        .file_size = completed.file_size,
        .chunk_size = completed.chunk_size,
        .available = true,
    };
    store_.save_file_meta(meta);
    metrics_.increment("file_upload_complete");
    return meta;
}

platform::common::ResumeToken FileService::query_resume_state(const std::string& upload_id) const {
    const auto session = store_.get_upload_session(upload_id);
    if (!session.has_value()) {
        throw std::runtime_error("unknown upload session");
    }

    platform::common::ResumeToken token {.upload_id = upload_id};
    const auto total = total_chunks(session->file_size, session->chunk_size);
    for (std::uint32_t chunk = 0; chunk < total; ++chunk) {
        if (std::find(session->uploaded_chunks.begin(), session->uploaded_chunks.end(), chunk) ==
            session->uploaded_chunks.end()) {
            token.missing_chunks.push_back(chunk);
        }
    }
    token.next_chunk = token.missing_chunks.empty() ? total : token.missing_chunks.front();
    return token;
}

std::vector<std::uint8_t> FileService::download_chunk(const std::string& file_id, std::uint32_t chunk_no) const {
    if (const auto hot = store_.get_hot_chunk(file_id, chunk_no); hot.has_value()) {
        return *hot;
    }
    if (const auto file_it = chunk_payloads_.find(file_id); file_it != chunk_payloads_.end()) {
        if (const auto chunk_it = file_it->second.find(chunk_no); chunk_it != file_it->second.end()) {
            return chunk_it->second;
        }
    }
    return {};
}

std::vector<platform::common::FileMeta> FileService::files() const {
    return store_.list_files();
}

std::vector<platform::common::FileChunkMeta> FileService::chunks(const std::string& file_id) const {
    return store_.list_chunks(file_id);
}

const platform::observability::MetricsRegistry& FileService::metrics() const {
    return metrics_;
}

std::string FileService::checksum(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t hash = 0;
    for (const auto byte : bytes) {
        hash = (hash * 16777619U) ^ byte;
    }
    std::ostringstream stream;
    stream << std::hex << std::setw(8) << std::setfill('0') << hash;
    return stream.str();
}

std::uint32_t FileService::total_chunks(std::uint64_t file_size, std::uint32_t chunk_size) {
    if (file_size == 0 || chunk_size == 0) {
        return 0;
    }
    return static_cast<std::uint32_t>((file_size + chunk_size - 1) / chunk_size);
}

}  // namespace file_service
