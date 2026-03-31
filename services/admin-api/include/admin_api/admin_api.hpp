#pragma once

#include "file_service/file_service.hpp"
#include "message_service/message_service.hpp"
#include "scheduler/scheduler_service.hpp"

#include <string>

namespace admin_api {

class AdminApi {
public:
    AdminApi(
        scheduler::SchedulerService& scheduler,
        message_service::MessageService& message_service,
        file_service::FileService& file_service);

    [[nodiscard]] std::string cluster_overview_json() const;
    [[nodiscard]] std::string route_stats_json() const;
    [[nodiscard]] std::string message_stats_json() const;
    [[nodiscard]] std::string file_stats_json() const;

private:
    scheduler::SchedulerService& scheduler_;
    message_service::MessageService& message_service_;
    file_service::FileService& file_service_;
};

}  // namespace admin_api

