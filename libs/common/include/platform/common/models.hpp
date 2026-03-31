#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace platform::common {

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

enum class ServiceKind {
    Gateway,
    Scheduler,
    Message,
    File,
    Admin
};

enum class RouteTarget {
    Message,
    File
};

enum class CommandType : std::uint16_t {
    Auth = 1,
    SendMessage = 2,
    PullOffline = 3,
    InitUpload = 4,
    UploadChunk = 5,
    CompleteUpload = 6,
    QueryFile = 7,
    DownloadChunk = 8,
    ResumeUpload = 9,
    Ack = 10
};

struct LoadScore {
    double cpu_percent {0.0};
    std::uint32_t active_connections {0};
    std::uint32_t queue_depth {0};

    [[nodiscard]] double weighted() const;
};

struct NodeDescriptor {
    std::string node_id;
    ServiceKind service {ServiceKind::Gateway};
    std::string host;
    std::uint16_t port {0};
    LoadScore load {};
    bool healthy {true};
    TimePoint last_heartbeat {Clock::now()};
};

struct RouteDecision {
    std::string route_key;
    RouteTarget target {RouteTarget::Message};
    std::optional<NodeDescriptor> node;
    std::string reason;
};

struct MessageEnvelope {
    std::string message_id;
    std::string conversation_id;
    std::string sender_id;
    std::string recipient_id;
    std::string body;
    bool is_group {false};
    TimePoint created_at {Clock::now()};
};

struct UnreadCounter {
    std::string user_id;
    std::string conversation_id;
    std::uint32_t unread {0};
};

struct UploadSession {
    std::string upload_id;
    std::string file_id;
    std::string owner_id;
    std::string file_name;
    std::uint64_t file_size {0};
    std::uint32_t chunk_size {4U * 1024U * 1024U};
    bool completed {false};
    std::vector<std::uint32_t> uploaded_chunks;
};

struct FileMeta {
    std::string file_id;
    std::string owner_id;
    std::string file_name;
    std::uint64_t file_size {0};
    std::uint32_t chunk_size {4U * 1024U * 1024U};
    bool available {false};
};

struct FileChunkMeta {
    std::string file_id;
    std::uint32_t chunk_no {0};
    std::uint64_t offset {0};
    std::uint32_t size {0};
    std::string checksum;
    std::string node_id;
};

struct ResumeToken {
    std::string upload_id;
    std::vector<std::uint32_t> missing_chunks;
    std::uint32_t next_chunk {0};
};

struct ClusterSnapshot {
    std::vector<NodeDescriptor> nodes;
    std::vector<RouteDecision> routes;
    std::vector<UnreadCounter> unread_counters;
    std::vector<FileMeta> files;
    std::vector<FileChunkMeta> chunks;
};

std::string to_string(ServiceKind kind);
std::string to_string(RouteTarget target);
std::string timestamp_string(TimePoint value);

}  // namespace platform::common

