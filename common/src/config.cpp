#include "dist_platform/config.hpp"

#include <fstream>
#include <stdexcept>

namespace dist_platform {

Config Config::LoadFile(const std::string& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("failed to open config: " + path);
  }
  nlohmann::json data;
  stream >> data;
  return Config(std::move(data));
}

Config::Config(nlohmann::json data) : data_(std::move(data)) {}

std::string Config::GetString(const std::string& key) const { return data_.at(key).get<std::string>(); }
int Config::GetInt(const std::string& key) const { return data_.at(key).get<int>(); }
uint16_t Config::GetUint16(const std::string& key) const { return data_.at(key).get<uint16_t>(); }
uint32_t Config::GetUint32(const std::string& key) const { return data_.at(key).get<uint32_t>(); }
double Config::GetDouble(const std::string& key) const { return data_.at(key).get<double>(); }
const nlohmann::json& Config::Raw() const { return data_; }

}  // namespace dist_platform