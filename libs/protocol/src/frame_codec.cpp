#include "platform/protocol/frame_codec.hpp"

#include <cstring>

namespace platform::protocol {

namespace {

template <typename T>
void append_numeric(std::vector<std::uint8_t>& out, T value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), raw, raw + sizeof(T));
}

template <typename T>
T read_numeric(const std::vector<std::uint8_t>& bytes, std::size_t& cursor) {
    T value {};
    std::memcpy(&value, bytes.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return value;
}

}  // namespace

std::vector<std::uint8_t> FrameCodec::encode(const Frame& frame) {
    std::vector<std::uint8_t> out;
    append_numeric(out, frame.version);
    append_numeric(out, static_cast<std::uint16_t>(frame.command));
    append_numeric(out, frame.request_id);

    const auto user_size = static_cast<std::uint32_t>(frame.user_id.size());
    const auto body_size = static_cast<std::uint32_t>(frame.body.size());
    append_numeric(out, user_size);
    append_numeric(out, body_size);
    out.insert(out.end(), frame.user_id.begin(), frame.user_id.end());
    out.insert(out.end(), frame.body.begin(), frame.body.end());
    append_numeric(out, checksum(frame.user_id + frame.body));
    return out;
}

std::optional<Frame> FrameCodec::decode(const std::vector<std::uint8_t>& bytes) {
    const std::size_t minimum = sizeof(std::uint16_t) * 2 + sizeof(std::uint64_t) + sizeof(std::uint32_t) * 3;
    if (bytes.size() < minimum) {
        return std::nullopt;
    }

    std::size_t cursor = 0;
    Frame frame;
    frame.version = read_numeric<std::uint16_t>(bytes, cursor);
    frame.command = static_cast<platform::common::CommandType>(read_numeric<std::uint16_t>(bytes, cursor));
    frame.request_id = read_numeric<std::uint64_t>(bytes, cursor);
    const auto user_size = read_numeric<std::uint32_t>(bytes, cursor);
    const auto body_size = read_numeric<std::uint32_t>(bytes, cursor);

    if (cursor + user_size + body_size + sizeof(std::uint32_t) > bytes.size()) {
        return std::nullopt;
    }

    frame.user_id.assign(reinterpret_cast<const char*>(bytes.data() + cursor), user_size);
    cursor += user_size;
    frame.body.assign(reinterpret_cast<const char*>(bytes.data() + cursor), body_size);
    cursor += body_size;
    frame.crc32 = read_numeric<std::uint32_t>(bytes, cursor);

    if (frame.crc32 != checksum(frame.user_id + frame.body)) {
        return std::nullopt;
    }
    return frame;
}

std::uint32_t FrameCodec::checksum(const std::string& bytes) {
    std::uint32_t value = 0;
    for (const auto ch : bytes) {
        value = (value * 131) + static_cast<unsigned char>(ch);
    }
    return value;
}

}  // namespace platform::protocol

