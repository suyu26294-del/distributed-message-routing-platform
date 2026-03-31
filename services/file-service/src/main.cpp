#include "file_service/file_service.hpp"

#include <iostream>

int main() {
    platform::storage::InMemoryStore store;
    file_service::FileService service(store);
    const auto session = service.init_upload("u1000", "demo.bin", 1024);
    const auto meta = service.upload_chunk({.upload_id = session.upload_id, .chunk_no = 0, .bytes = {1, 2, 3}, .node_id = "file-a"});
    std::cout << "file-service ready; first chunk checksum=" << meta.checksum << "\n";
    return 0;
}

