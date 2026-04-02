#pragma once

#include <cstdint>
#include <string>

namespace dist_platform {

uint32_t ChunkCountFromSize(uint64_t file_size, uint32_t chunk_size);
void EnsureDir(const std::string& path);
std::string SanitizeFileName(const std::string& name);
std::string PartPath(const std::string& data_dir, const std::string& transfer_id);
std::string FinalPath(const std::string& data_dir, const std::string& transfer_id, const std::string& file_name);
void WriteChunkAt(const std::string& path, uint32_t chunk_index, uint32_t chunk_size, const std::string& data);
std::string ReadChunkAt(const std::string& path, uint32_t chunk_index, uint32_t chunk_size);
bool FileExists(const std::string& path);
void FinalizeTransfer(const std::string& part_path, const std::string& final_path);

}  // namespace dist_platform