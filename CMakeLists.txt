cmake_minimum_required(VERSION 3.20)
project(distributed_platform_v1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLATFORM_BUILD_TESTS "Build platform tests" ON)

add_library(platform_common
    libs/common/src/models.cpp
)
target_include_directories(platform_common PUBLIC libs/common/include)

add_library(platform_routing
    libs/routing/src/consistent_hash_router.cpp
)
target_include_directories(platform_routing PUBLIC libs/routing/include)
target_link_libraries(platform_routing PUBLIC platform_common)

add_library(platform_storage
    libs/storage/src/in_memory_store.cpp
)
target_include_directories(platform_storage PUBLIC libs/storage/include)
target_link_libraries(platform_storage PUBLIC platform_common)

add_library(platform_protocol
    libs/protocol/src/frame_codec.cpp
)
target_include_directories(platform_protocol PUBLIC libs/protocol/include)
target_link_libraries(platform_protocol PUBLIC platform_common)

add_library(platform_net
    libs/net/src/reactor_stub.cpp
)
target_include_directories(platform_net PUBLIC libs/net/include)
target_link_libraries(platform_net PUBLIC platform_common platform_protocol)

add_library(platform_observability
    libs/observability/src/metrics_registry.cpp
)
target_include_directories(platform_observability PUBLIC libs/observability/include)
target_link_libraries(platform_observability PUBLIC platform_common)

add_library(scheduler_lib
    services/scheduler/src/scheduler_service.cpp
)
target_include_directories(scheduler_lib PUBLIC services/scheduler/include)
target_link_libraries(scheduler_lib PUBLIC
    platform_common platform_routing platform_observability
)

add_library(message_service_lib
    services/message-service/src/message_service.cpp
)
target_include_directories(message_service_lib PUBLIC services/message-service/include)
target_link_libraries(message_service_lib PUBLIC
    platform_common platform_storage platform_observability
)

add_library(file_service_lib
    services/file-service/src/file_service.cpp
)
target_include_directories(file_service_lib PUBLIC services/file-service/include)
target_link_libraries(file_service_lib PUBLIC
    platform_common platform_storage platform_observability
)

add_library(gateway_lib
    services/gateway/src/gateway_service.cpp
)
target_include_directories(gateway_lib PUBLIC services/gateway/include)
target_link_libraries(gateway_lib PUBLIC
    platform_common platform_protocol platform_routing platform_storage platform_observability platform_net
    scheduler_lib message_service_lib file_service_lib
)

add_library(admin_api_lib
    services/admin-api/src/admin_api.cpp
)
target_include_directories(admin_api_lib PUBLIC services/admin-api/include)
target_link_libraries(admin_api_lib PUBLIC
    platform_common platform_observability platform_routing platform_storage
    scheduler_lib message_service_lib file_service_lib
)

add_executable(gateway_service services/gateway/src/main.cpp)
target_link_libraries(gateway_service PRIVATE gateway_lib)

add_executable(scheduler_service services/scheduler/src/main.cpp)
target_link_libraries(scheduler_service PRIVATE scheduler_lib)

add_executable(message_service services/message-service/src/main.cpp)
target_link_libraries(message_service PRIVATE message_service_lib)

add_executable(file_service services/file-service/src/main.cpp)
target_link_libraries(file_service PRIVATE file_service_lib)

add_executable(admin_api services/admin-api/src/main.cpp)
target_link_libraries(admin_api PRIVATE admin_api_lib)

add_executable(demo_client
    clients/demo-client/src/main.cpp
)
target_link_libraries(demo_client PRIVATE
    platform_common platform_routing platform_storage platform_protocol platform_observability
    gateway_lib scheduler_lib message_service_lib file_service_lib admin_api_lib
)

if(PLATFORM_BUILD_TESTS)
    enable_testing()

    add_executable(unit_routing_test
        tests/unit/test_routing.cpp
    )
    target_link_libraries(unit_routing_test PRIVATE platform_common platform_routing)
    add_test(NAME unit_routing_test COMMAND unit_routing_test)

    add_executable(unit_protocol_test
        tests/unit/test_protocol.cpp
    )
    target_link_libraries(unit_protocol_test PRIVATE platform_common platform_protocol)
    add_test(NAME unit_protocol_test COMMAND unit_protocol_test)

    add_executable(integration_message_file_test
        tests/integration/test_message_file.cpp
    )
    target_link_libraries(integration_message_file_test PRIVATE
        platform_common platform_routing platform_storage platform_observability
        gateway_lib scheduler_lib message_service_lib file_service_lib
    )
    add_test(NAME integration_message_file_test COMMAND integration_message_file_test)
endif()
