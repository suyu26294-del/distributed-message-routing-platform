#pragma once

#include <string>
#include <string_view>

namespace dist_platform {

std::string EncodeFrame(std::string_view payload);
bool TryDecodeFrame(std::string& buffer, std::string& payload);

}  // namespace dist_platform