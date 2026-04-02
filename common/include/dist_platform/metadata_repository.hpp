#pragma once

#include "dist_platform/mysql_store.hpp"
#include "dist_platform.pb.h"

#include <cstdint>
#include <string>
#include <vector>

namespace dist_platform {

struct TransferQueryResult {
  bool found = false;
  distplatform::FileManifest manifest;
  std::vector<uint32_t> chunk_indices;
};

struct MetadataCounts {
  uint32_t stored_offline_messages = 0;
  uint32_t active_transfers = 0;
};

class MetadataRepository {
 public:
  explicit MetadataRepository(MySqlStore& store);

  void EnsureSchema();
  void EnsureUser(const std::string& user_id);
  void PersistMessage(const distplatform::RouteEnvelope& envelope, const std::string& state);
  void UpdateMessageState(const std::string& message_id, const std::string& state);
  void PersistFile(const distplatform::FileManifest& manifest, const std::string& state);
  void RecordChunk(const std::string& transfer_id, uint32_t chunk_index, uint32_t chunk_size);
  TransferQueryResult QueryTransfer(const std::string& transfer_id);
  MetadataCounts SummaryCounts();

 private:
  MySqlStore& store_;
};

}  // namespace dist_platform