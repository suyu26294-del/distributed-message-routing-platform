#pragma once

#include "platform/common/models.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace platform::protocol {

struct Frame {
    std::uint16_t version {1};
    platform::common::CommandType command {platform::common::CommandType::Auth};
    std::uint64_t request_id {0};
    std::string user_id;
    std::string body;
    std::uint32_t crc32 {0};
};

class FrameCodec {
public:
    [[nodiscard]] static std::vector<std::uint8_t> encode(const Frame& frame);
    [[nodiscard]] static std::optional<Frame> decode(const std::vector<std::uint8_t>& bytes);

private:
    [[nodiscard]] static std::uint32_t checksum(const std::string& bytes);
};

}  // namespace platform::protocol

