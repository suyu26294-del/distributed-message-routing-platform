#pragma once

#include <string>
#include <vector>

namespace dist_platform {

std::string Base64Encode(const std::string& input);
std::string Base64Decode(const std::string& input);

}  // namespace dist_platform