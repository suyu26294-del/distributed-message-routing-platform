#include "dist_platform/redis_client.hpp"

#include <hiredis/hiredis.h>

#include <stdexcept>

namespace dist_platform {
namespace {

redisReply* CheckedReply(redisReply* reply, redisContext* context) {
  if (reply == nullptr) {
    throw std::runtime_error(context != nullptr && context->errstr != nullptr ? context->errstr : "redis reply failed");
  }
  return reply;
}

}  // namespace

RedisClient::RedisClient(std::string host, int port) : host_(std::move(host)), port_(port), context_(nullptr) {}

RedisClient::~RedisClient() {
  if (context_ != nullptr) {
    redisFree(static_cast<redisContext*>(context_));
  }
}

void RedisClient::Connect() {
  if (context_ != nullptr) {
    redisFree(static_cast<redisContext*>(context_));
    context_ = nullptr;
  }
  redisContext* context = redisConnect(host_.c_str(), port_);
  if (context == nullptr || context->err) {
    const std::string error = context != nullptr ? context->errstr : "redis connect failed";
    if (context != nullptr) {
      redisFree(context);
    }
    throw std::runtime_error(error);
  }
  context_ = context;
}

void RedisClient::EnsureConnected() {
  if (context_ == nullptr) {
    Connect();
  }
}

void RedisClient::Ping() {
  EnsureConnected();
  freeReplyObject(CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_), "PING")),
                               static_cast<redisContext*>(context_)));
}

void RedisClient::SetEx(const std::string& key, int ttl_seconds, const std::string& value) {
  EnsureConnected();
  freeReplyObject(CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_),
                                                                     "SETEX %s %d %s",
                                                                     key.c_str(),
                                                                     ttl_seconds,
                                                                     value.c_str())),
                               static_cast<redisContext*>(context_)));
}

std::optional<std::string> RedisClient::Get(const std::string& key) {
  EnsureConnected();
  redisReply* reply = CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_), "GET %s", key.c_str())),
                                   static_cast<redisContext*>(context_));
  std::optional<std::string> result;
  if (reply->type == REDIS_REPLY_STRING) {
    result = std::string(reply->str, reply->len);
  }
  freeReplyObject(reply);
  return result;
}

void RedisClient::Del(const std::string& key) {
  EnsureConnected();
  freeReplyObject(CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_), "DEL %s", key.c_str())),
                               static_cast<redisContext*>(context_)));
}

void RedisClient::HSet(const std::string& key, const std::unordered_map<std::string, std::string>& values) {
  EnsureConnected();
  for (const auto& [field, value] : values) {
    freeReplyObject(CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_),
                                                                       "HSET %s %s %s",
                                                                       key.c_str(),
                                                                       field.c_str(),
                                                                       value.c_str())),
                                 static_cast<redisContext*>(context_)));
  }
}

std::unordered_map<std::string, std::string> RedisClient::HGetAll(const std::string& key) {
  EnsureConnected();
  redisReply* reply = CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_), "HGETALL %s", key.c_str())),
                                   static_cast<redisContext*>(context_));
  std::unordered_map<std::string, std::string> result;
  if (reply->type == REDIS_REPLY_ARRAY) {
    for (size_t i = 0; i + 1 < reply->elements; i += 2) {
      result.emplace(reply->element[i]->str, reply->element[i + 1]->str);
    }
  }
  freeReplyObject(reply);
  return result;
}

void RedisClient::Expire(const std::string& key, int ttl_seconds) {
  EnsureConnected();
  freeReplyObject(CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_),
                                                                     "EXPIRE %s %d",
                                                                     key.c_str(),
                                                                     ttl_seconds)),
                               static_cast<redisContext*>(context_)));
}

void RedisClient::LPush(const std::string& key, const std::string& value) {
  EnsureConnected();
  freeReplyObject(CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_),
                                                                     "LPUSH %s %s",
                                                                     key.c_str(),
                                                                     value.c_str())),
                               static_cast<redisContext*>(context_)));
}

std::vector<std::string> RedisClient::LRange(const std::string& key, int start, int stop) {
  EnsureConnected();
  redisReply* reply = CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_),
                                                                         "LRANGE %s %d %d",
                                                                         key.c_str(),
                                                                         start,
                                                                         stop)),
                                   static_cast<redisContext*>(context_));
  std::vector<std::string> items;
  if (reply->type == REDIS_REPLY_ARRAY) {
    items.reserve(reply->elements);
    for (size_t i = 0; i < reply->elements; ++i) {
      items.emplace_back(reply->element[i]->str, reply->element[i]->len);
    }
  }
  freeReplyObject(reply);
  return items;
}

long long RedisClient::LLen(const std::string& key) {
  EnsureConnected();
  redisReply* reply = CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_), "LLEN %s", key.c_str())),
                                   static_cast<redisContext*>(context_));
  const long long value = reply->type == REDIS_REPLY_INTEGER ? reply->integer : 0;
  freeReplyObject(reply);
  return value;
}

std::vector<std::string> RedisClient::Keys(const std::string& pattern) {
  EnsureConnected();
  redisReply* reply = CheckedReply(static_cast<redisReply*>(redisCommand(static_cast<redisContext*>(context_), "KEYS %s", pattern.c_str())),
                                   static_cast<redisContext*>(context_));
  std::vector<std::string> items;
  if (reply->type == REDIS_REPLY_ARRAY) {
    items.reserve(reply->elements);
    for (size_t i = 0; i < reply->elements; ++i) {
      items.emplace_back(reply->element[i]->str, reply->element[i]->len);
    }
  }
  freeReplyObject(reply);
  return items;
}

}  // namespace dist_platform