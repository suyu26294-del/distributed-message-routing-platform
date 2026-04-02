#include "dist_platform/metadata_repository.hpp"

#include <sstream>

namespace dist_platform {
namespace {

uint32_t ToUint(const std::string& value) {
  return value.empty() ? 0U : static_cast<uint32_t>(std::stoul(value));
}

uint64_t ToUint64(const std::string& value) {
  return value.empty() ? 0ULL : std::stoull(value);
}

}  // namespace

MetadataRepository::MetadataRepository(MySqlStore& store) : store_(store) {}

void MetadataRepository::EnsureSchema() {
  store_.Execute(
      "CREATE TABLE IF NOT EXISTS users ("
      " user_id VARCHAR(64) PRIMARY KEY,"
      " created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
      " last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  store_.Execute(
      "CREATE TABLE IF NOT EXISTS messages ("
      " message_id VARCHAR(96) PRIMARY KEY,"
      " from_user VARCHAR(64) NOT NULL,"
      " to_user VARCHAR(64) NOT NULL,"
      " body TEXT NOT NULL,"
      " state VARCHAR(32) NOT NULL,"
      " created_at_ms BIGINT NOT NULL,"
      " updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  store_.Execute(
      "CREATE TABLE IF NOT EXISTS file_transfers ("
      " transfer_id VARCHAR(96) PRIMARY KEY,"
      " owner_user VARCHAR(64) NOT NULL,"
      " file_name VARCHAR(255) NOT NULL,"
      " file_size BIGINT UNSIGNED NOT NULL,"
      " chunk_size INT UNSIGNED NOT NULL,"
      " total_chunks INT UNSIGNED NOT NULL,"
      " sha256 VARCHAR(128) NOT NULL,"
      " state VARCHAR(32) NOT NULL,"
      " created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
      " updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
  store_.Execute(
      "CREATE TABLE IF NOT EXISTS file_chunks ("
      " transfer_id VARCHAR(96) NOT NULL,"
      " chunk_index INT UNSIGNED NOT NULL,"
      " chunk_size INT UNSIGNED NOT NULL,"
      " created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
      " PRIMARY KEY (transfer_id, chunk_index)"
      ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");
}

void MetadataRepository::EnsureUser(const std::string& user_id) {
  const std::string escaped = store_.Escape(user_id);
  store_.Execute("INSERT INTO users(user_id) VALUES('" + escaped + "') ON DUPLICATE KEY UPDATE last_seen = CURRENT_TIMESTAMP");
}

void MetadataRepository::PersistMessage(const distplatform::RouteEnvelope& envelope, const std::string& state) {
  const std::string message_id = store_.Escape(envelope.message_id());
  const std::string from_user = store_.Escape(envelope.from_user());
  const std::string to_user = store_.Escape(envelope.to_user());
  const std::string body = store_.Escape(envelope.body());
  const std::string escaped_state = store_.Escape(state);
  std::ostringstream sql;
  sql << "INSERT INTO messages(message_id, from_user, to_user, body, state, created_at_ms) VALUES('"
      << message_id << "','" << from_user << "','" << to_user << "','" << body << "','" << escaped_state << "',"
      << envelope.created_at_ms() << ") ON DUPLICATE KEY UPDATE state='" << escaped_state << "', body='" << body << "'";
  store_.Execute(sql.str());
}

void MetadataRepository::UpdateMessageState(const std::string& message_id, const std::string& state) {
  store_.Execute("UPDATE messages SET state='" + store_.Escape(state) + "' WHERE message_id='" + store_.Escape(message_id) + "'");
}

void MetadataRepository::PersistFile(const distplatform::FileManifest& manifest, const std::string& state) {
  std::ostringstream sql;
  sql << "INSERT INTO file_transfers(transfer_id, owner_user, file_name, file_size, chunk_size, total_chunks, sha256, state) VALUES('"
      << store_.Escape(manifest.transfer_id()) << "','" << store_.Escape(manifest.owner_user()) << "','"
      << store_.Escape(manifest.file_name()) << "'," << manifest.file_size() << "," << manifest.chunk_size() << ","
      << manifest.total_chunks() << ", '" << store_.Escape(manifest.sha256()) << "','" << store_.Escape(state)
      << "') ON DUPLICATE KEY UPDATE file_name=VALUES(file_name), file_size=VALUES(file_size), chunk_size=VALUES(chunk_size), total_chunks=VALUES(total_chunks), sha256=VALUES(sha256), state=VALUES(state)";
  store_.Execute(sql.str());
}

void MetadataRepository::RecordChunk(const std::string& transfer_id, uint32_t chunk_index, uint32_t chunk_size) {
  std::ostringstream insert;
  insert << "INSERT IGNORE INTO file_chunks(transfer_id, chunk_index, chunk_size) VALUES('" << store_.Escape(transfer_id)
         << "'," << chunk_index << "," << chunk_size << ")";
  store_.Execute(insert.str());

  const auto transfer = QueryTransfer(transfer_id);
  if (transfer.found && transfer.chunk_indices.size() >= transfer.manifest.total_chunks()) {
    store_.Execute("UPDATE file_transfers SET state='completed' WHERE transfer_id='" + store_.Escape(transfer_id) + "'");
  }
}

TransferQueryResult MetadataRepository::QueryTransfer(const std::string& transfer_id) {
  TransferQueryResult result;
  const auto transfer_rows = store_.Query(
      "SELECT transfer_id, owner_user, file_name, file_size, chunk_size, total_chunks, sha256 FROM file_transfers WHERE transfer_id='" +
      store_.Escape(transfer_id) + "'");
  if (transfer_rows.empty()) {
    return result;
  }
  const auto& row = transfer_rows.front();
  result.found = true;
  result.manifest.set_transfer_id(row.at("transfer_id"));
  result.manifest.set_owner_user(row.at("owner_user"));
  result.manifest.set_file_name(row.at("file_name"));
  result.manifest.set_file_size(ToUint64(row.at("file_size")));
  result.manifest.set_chunk_size(ToUint(row.at("chunk_size")));
  result.manifest.set_total_chunks(ToUint(row.at("total_chunks")));
  result.manifest.set_sha256(row.at("sha256"));

  const auto chunk_rows = store_.Query(
      "SELECT chunk_index FROM file_chunks WHERE transfer_id='" + store_.Escape(transfer_id) + "' ORDER BY chunk_index ASC");
  for (const auto& chunk_row : chunk_rows) {
    result.chunk_indices.push_back(ToUint(chunk_row.at("chunk_index")));
  }
  return result;
}

MetadataCounts MetadataRepository::SummaryCounts() {
  MetadataCounts counts;
  const auto offline_rows = store_.Query("SELECT COUNT(*) AS c FROM messages WHERE state='offline'");
  if (!offline_rows.empty()) {
    counts.stored_offline_messages = ToUint(offline_rows.front().at("c"));
  }
  const auto transfer_rows = store_.Query("SELECT COUNT(*) AS c FROM file_transfers WHERE state <> 'completed'");
  if (!transfer_rows.empty()) {
    counts.active_transfers = ToUint(transfer_rows.front().at("c"));
  }
  return counts;
}

}  // namespace dist_platform