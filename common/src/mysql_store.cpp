#include "dist_platform/mysql_store.hpp"

#include <mysql/mysql.h>

#include <stdexcept>

namespace dist_platform {

MySqlStore::MySqlStore(std::string host, uint16_t port, std::string user, std::string password, std::string database)
    : host_(std::move(host)),
      port_(port),
      user_(std::move(user)),
      password_(std::move(password)),
      database_(std::move(database)),
      handle_(nullptr) {}

MySqlStore::~MySqlStore() {
  if (handle_ != nullptr) {
    mysql_close(static_cast<MYSQL*>(handle_));
  }
}

void MySqlStore::Connect() {
  if (handle_ != nullptr) {
    mysql_close(static_cast<MYSQL*>(handle_));
    handle_ = nullptr;
  }
  MYSQL* mysql = mysql_init(nullptr);
  if (mysql == nullptr) {
    throw std::runtime_error("mysql_init failed");
  }
  if (mysql_real_connect(mysql,
                         host_.c_str(),
                         user_.c_str(),
                         password_.c_str(),
                         database_.c_str(),
                         port_,
                         nullptr,
                         0) == nullptr) {
    const std::string error = mysql_error(mysql);
    mysql_close(mysql);
    throw std::runtime_error("mysql_real_connect failed: " + error);
  }
  handle_ = mysql;
}

void MySqlStore::EnsureConnected() {
  if (handle_ == nullptr) {
    Connect();
  }
}

void MySqlStore::Execute(const std::string& sql) {
  EnsureConnected();
  if (mysql_query(static_cast<MYSQL*>(handle_), sql.c_str()) != 0) {
    throw std::runtime_error("mysql execute failed: " + std::string(mysql_error(static_cast<MYSQL*>(handle_))));
  }
}

std::vector<QueryRow> MySqlStore::Query(const std::string& sql) {
  EnsureConnected();
  if (mysql_query(static_cast<MYSQL*>(handle_), sql.c_str()) != 0) {
    throw std::runtime_error("mysql query failed: " + std::string(mysql_error(static_cast<MYSQL*>(handle_))));
  }
  MYSQL_RES* result = mysql_store_result(static_cast<MYSQL*>(handle_));
  if (result == nullptr) {
    return {};
  }
  std::vector<QueryRow> rows;
  MYSQL_ROW row;
  MYSQL_FIELD* fields = mysql_fetch_fields(result);
  const unsigned int field_count = mysql_num_fields(result);
  while ((row = mysql_fetch_row(result)) != nullptr) {
    unsigned long* lengths = mysql_fetch_lengths(result);
    QueryRow values;
    for (unsigned int i = 0; i < field_count; ++i) {
      const std::string key = fields[i].name;
      const std::string value = row[i] == nullptr ? std::string() : std::string(row[i], lengths[i]);
      values.emplace(key, value);
    }
    rows.push_back(std::move(values));
  }
  mysql_free_result(result);
  return rows;
}

std::string MySqlStore::Escape(const std::string& value) {
  EnsureConnected();
  std::string escaped(value.size() * 2 + 1, '\0');
  const unsigned long length = mysql_real_escape_string(static_cast<MYSQL*>(handle_),
                                                        escaped.data(),
                                                        value.c_str(),
                                                        value.size());
  escaped.resize(length);
  return escaped;
}

}  // namespace dist_platform