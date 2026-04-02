#include "dist_platform/file_utils.hpp"

#include <filesystem>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace dist_platform {
namespace {

void WriteAllAt(int fd, const char* data, size_t size, off_t offset) {
  size_t written = 0;
  while (written < size) {
    const ssize_t rc = pwrite(fd, data + written, size - written, offset + static_cast<off_t>(written));
    if (rc < 0) {
      throw std::runtime_error("pwrite failed");
    }
    written += static_cast<size_t>(rc);
  }
}

std::string ReadAllAt(int fd, size_t size, off_t offset) {
  std::string result(size, '\0');
  size_t total = 0;
  while (total < size) {
    const ssize_t rc = pread(fd, result.data() + total, size - total, offset + static_cast<off_t>(total));
    if (rc < 0) {
      throw std::runtime_error("pread failed");
    }
    if (rc == 0) {
      result.resize(total);
      break;
    }
    total += static_cast<size_t>(rc);
  }
  return result;
}

}  // namespace

uint32_t ChunkCountFromSize(uint64_t file_size, uint32_t chunk_size) {
  if (chunk_size == 0) {
    throw std::runtime_error("chunk_size must be > 0");
  }
  return static_cast<uint32_t>((file_size + chunk_size - 1) / chunk_size);
}

void EnsureDir(const std::string& path) { std::filesystem::create_directories(path); }

std::string SanitizeFileName(const std::string& name) {
  std::string result = name;
  for (char& ch : result) {
    const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                         (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-';
    if (!allowed) {
      ch = '_';
    }
  }
  return result;
}

std::string PartPath(const std::string& data_dir, const std::string& transfer_id) {
  return data_dir + "/" + transfer_id + ".part";
}

std::string FinalPath(const std::string& data_dir, const std::string& transfer_id, const std::string& file_name) {
  return data_dir + "/" + transfer_id + "-" + SanitizeFileName(file_name);
}

void WriteChunkAt(const std::string& path, uint32_t chunk_index, uint32_t chunk_size, const std::string& data) {
  const int fd = open(path.c_str(), O_CREAT | O_WRONLY, 0644);
  if (fd < 0) {
    throw std::runtime_error("failed to open chunk file");
  }
  WriteAllAt(fd, data.data(), data.size(), static_cast<off_t>(chunk_index) * chunk_size);
  close(fd);
}

std::string ReadChunkAt(const std::string& path, uint32_t chunk_index, uint32_t chunk_size) {
  const int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) {
    throw std::runtime_error("failed to open file for read");
  }
  const std::string data = ReadAllAt(fd, chunk_size, static_cast<off_t>(chunk_index) * chunk_size);
  close(fd);
  return data;
}

bool FileExists(const std::string& path) { return std::filesystem::exists(path); }

void FinalizeTransfer(const std::string& part_path, const std::string& final_path) {
  std::filesystem::rename(part_path, final_path);
}

}  // namespace dist_platform