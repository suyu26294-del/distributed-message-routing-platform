#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

namespace dist_platform {

class Config {
 public:
  static Config LoadFile(const std::string& path);

  explicit Config(nlohmann::json data);

  std::string GetString(const std::string& key) const;
  int GetInt(const std::string& key) const;
  uint16_t GetUint16(const std::string& key) const;
  uint32_t GetUint32(const std::string& key) const;
  double GetDouble(const std::string& key) const;
  const nlohmann::json& Raw() const;

 private:
  nlohmann::json data_;
};

}  // namespace dist_platform