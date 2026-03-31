#include "admin_api/admin_api.hpp"

#include <iostream>

int main() {
    platform::storage::InMemoryStore store;
    scheduler::SchedulerService scheduler;
    message_service::MessageService message_service(store);
    file_service::FileService file_service(store);
    admin_api::AdminApi api(scheduler, message_service, file_service);
    std::cout << "admin-api ready; cluster=" << api.cluster_overview_json() << "\n";
    return 0;
}
