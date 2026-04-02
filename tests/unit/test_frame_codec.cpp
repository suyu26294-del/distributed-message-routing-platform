#include "dist_platform/frame_codec.hpp"

#include <gtest/gtest.h>

TEST(FrameCodecTest, EncodesAndDecodesSingleFrame) {
  const std::string payload = "hello";
  std::string buffer = dist_platform::EncodeFrame(payload);
  std::string decoded;
  ASSERT_TRUE(dist_platform::TryDecodeFrame(buffer, decoded));
  EXPECT_EQ(decoded, payload);
  EXPECT_TRUE(buffer.empty());
}

TEST(FrameCodecTest, HandlesPartialFrameBuffer) {
  const std::string payload = "payload";
  std::string encoded = dist_platform::EncodeFrame(payload);
  std::string buffer = encoded.substr(0, 3);
  std::string decoded;
  EXPECT_FALSE(dist_platform::TryDecodeFrame(buffer, decoded));
  buffer += encoded.substr(3);
  ASSERT_TRUE(dist_platform::TryDecodeFrame(buffer, decoded));
  EXPECT_EQ(decoded, payload);
}