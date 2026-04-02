#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace dist_platform {

using QueryRow = std::unordered_map<std::string, std::string>;

class MySqlStore {
 public:
  MySqlStore(std::string host, uint16_t port, std::string user, std::string password, std::string database);
  ~MySqlStore();

  void Connect();
  void Execute(const std::string& sql);
  std::vector<QueryRow> Query(const std::string& sql);
  std::string Escape(const std::string& value);

 private:
  void EnsureConnected();

  std::string host_;
  uint16_t port_;
  std::string user_;
  std::string password_;
  std::string database_;
  void* handle_;
};

}  // namespace dist_platform