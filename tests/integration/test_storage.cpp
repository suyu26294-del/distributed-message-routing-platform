#include "dist_platform/metadata_repository.hpp"
#include "dist_platform/mysql_store.hpp"
#include "dist_platform/random_id.hpp"
#include "dist_platform/redis_client.hpp"
#include "dist_platform.pb.h"

#include <gtest/gtest.h>

TEST(StorageIntegrationTest, CanPersistMetadataAndUseRedis) {
  dist_platform::MySqlStore store("127.0.0.1", 3306, "dist_platform", "dist_platform", "dist_platform");
  ASSERT_NO_THROW(store.Connect());

  dist_platform::MetadataRepository repository(store);
  ASSERT_NO_THROW(repository.EnsureSchema());

  distplatform::RouteEnvelope envelope;
  envelope.set_message_id(dist_platform::RandomId("itest-msg"));
  envelope.set_from_user("alice");
  envelope.set_to_user("bob");
  envelope.set_body("hello storage");
  envelope.set_created_at_ms(123456789);
  ASSERT_NO_THROW(repository.PersistMessage(envelope, "offline"));
  ASSERT_NO_THROW(repository.UpdateMessageState(envelope.message_id(), "acked"));

  distplatform::FileManifest manifest;
  manifest.set_transfer_id(dist_platform::RandomId("itest-file"));
  manifest.set_owner_user("alice");
  manifest.set_file_name("sample.bin");
  manifest.set_file_size(1024);
  manifest.set_chunk_size(256);
  manifest.set_total_chunks(4);
  manifest.set_sha256("hash");
  ASSERT_NO_THROW(repository.PersistFile(manifest, "in_progress"));
  ASSERT_NO_THROW(repository.RecordChunk(manifest.transfer_id(), 0, 256));
  const auto transfer = repository.QueryTransfer(manifest.transfer_id());
  ASSERT_TRUE(transfer.found);
  EXPECT_EQ(transfer.chunk_indices.size(), 1U);

  dist_platform::RedisClient redis("127.0.0.1", 6379);
  ASSERT_NO_THROW(redis.Connect());
  const std::string hash_key = dist_platform::RandomId("itest-hash");
  const std::string list_key = dist_platform::RandomId("itest-list");
  redis.HSet(hash_key, {{"field", "value"}});
  const auto values = redis.HGetAll(hash_key);
  EXPECT_EQ(values.at("field"), "value");
  redis.LPush(list_key, "one");
  redis.LPush(list_key, "two");
  EXPECT_EQ(redis.LLen(list_key), 2);
  redis.Del(hash_key);
  redis.Del(list_key);

  store.Execute("DELETE FROM file_chunks WHERE transfer_id='" + store.Escape(manifest.transfer_id()) + "'");
  store.Execute("DELETE FROM file_transfers WHERE transfer_id='" + store.Escape(manifest.transfer_id()) + "'");
  store.Execute("DELETE FROM messages WHERE message_id='" + store.Escape(envelope.message_id()) + "'");
}