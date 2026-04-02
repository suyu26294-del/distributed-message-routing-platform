#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace dist_platform {

class RedisClient {
 public:
  RedisClient(std::string host, int port);
  ~RedisClient();

  void Connect();
  void Ping();
  void SetEx(const std::string& key, int ttl_seconds, const std::string& value);
  std::optional<std::string> Get(const std::string& key);
  void Del(const std::string& key);
  void HSet(const std::string& key, const std::unordered_map<std::string, std::string>& values);
  std::unordered_map<std::string, std::string> HGetAll(const std::string& key);
  void Expire(const std::string& key, int ttl_seconds);
  void LPush(const std::string& key, const std::string& value);
  std::vector<std::string> LRange(const std::string& key, int start, int stop);
  long long LLen(const std::string& key);
  std::vector<std::string> Keys(const std::string& pattern);

 private:
  void EnsureConnected();

  std::string host_;
  int port_;
  void* context_;
};

}  // namespace dist_platform