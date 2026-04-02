#include "dist_platform/base64.hpp"

#include <openssl/evp.h>

#include <stdexcept>
#include <vector>

namespace dist_platform {

std::string Base64Encode(const std::string& input) {
  if (input.empty()) {
    return {};
  }
  std::vector<unsigned char> output(4 * ((input.size() + 2) / 3) + 1);
  const int length = EVP_EncodeBlock(output.data(),
                                     reinterpret_cast<const unsigned char*>(input.data()),
                                     static_cast<int>(input.size()));
  return std::string(reinterpret_cast<char*>(output.data()), static_cast<size_t>(length));
}

std::string Base64Decode(const std::string& input) {
  if (input.empty()) {
    return {};
  }
  std::vector<unsigned char> output(input.size() + 1);
  int length = EVP_DecodeBlock(output.data(),
                               reinterpret_cast<const unsigned char*>(input.data()),
                               static_cast<int>(input.size()));
  if (length < 0) {
    throw std::runtime_error("invalid base64 input");
  }
  size_t padding = 0;
  if (!input.empty() && input.back() == '=') {
    ++padding;
  }
  if (input.size() > 1 && input[input.size() - 2] == '=') {
    ++padding;
  }
  return std::string(reinterpret_cast<char*>(output.data()), static_cast<size_t>(length) - padding);
}

}  // namespace dist_platform