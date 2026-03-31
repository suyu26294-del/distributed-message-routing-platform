#include "platform/common/models.hpp"
#include "platform/protocol/frame_codec.hpp"

#include <cstdlib>
#include <iostream>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "test failure: " << message << "\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    const platform::protocol::Frame original {
        .version = 1,
        .command = platform::common::CommandType::SendMessage,
        .request_id = 42,
        .user_id = "u1000",
        .body = "u1001|hello",
    };

    auto bytes = platform::protocol::FrameCodec::encode(original);
    const auto decoded = platform::protocol::FrameCodec::decode(bytes);
    expect(decoded.has_value(), "encoded frame should decode");
    expect(decoded->request_id == original.request_id, "request id should round-trip");
    expect(decoded->user_id == original.user_id, "user should round-trip");
    expect(decoded->body == original.body, "body should round-trip");

    bytes.back() ^= 0xFF;
    const auto corrupt = platform::protocol::FrameCodec::decode(bytes);
    expect(!corrupt.has_value(), "crc failure should reject frame");

    std::cout << "unit_protocol_test passed\n";
    return 0;
}

