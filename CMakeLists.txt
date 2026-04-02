cmake_minimum_required(VERSION 3.28)

project(dist_msg_platform VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DIST_PLATFORM_BUILD_TESTS "Build tests" ON)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(MYSQLCLIENT REQUIRED mysqlclient)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/dist_platform.proto)

add_library(distplatform_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(distplatform_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(distplatform_proto PUBLIC protobuf::libprotobuf)

add_library(distplatform_common STATIC
    common/src/base64.cpp
    common/src/config.cpp
    common/src/file_utils.cpp
    common/src/frame_codec.cpp
    common/src/logging.cpp
    common/src/metadata_repository.cpp
    common/src/mysql_store.cpp
    common/src/random_id.cpp
    common/src/redis_client.cpp
    common/src/router_logic.cpp
    common/src/rpc_client.cpp
    common/src/tcp_frame_server.cpp
    common/src/thread_pool.cpp
    common/src/time_utils.cpp
)

target_include_directories(distplatform_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${CMAKE_CURRENT_BINARY_DIR}
        ${HIREDIS_INCLUDE_DIRS}
        ${MYSQLCLIENT_INCLUDE_DIRS}
)

target_link_directories(distplatform_common
    PUBLIC
        ${HIREDIS_LIBRARY_DIRS}
        ${MYSQLCLIENT_LIBRARY_DIRS}
)

target_link_libraries(distplatform_common
    PUBLIC
        distplatform_proto
        Threads::Threads
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        protobuf::libprotobuf
        spdlog::spdlog
        ${HIREDIS_LIBRARIES}
        ${MYSQLCLIENT_LIBRARIES}
)

target_compile_options(distplatform_common PRIVATE ${HIREDIS_CFLAGS_OTHER} ${MYSQLCLIENT_CFLAGS_OTHER})

function(add_distplatform_executable target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE distplatform_common)
endfunction()

add_distplatform_executable(metadata_service services/metadata/main.cpp)
add_distplatform_executable(session_service services/session/main.cpp)
add_distplatform_executable(router_service services/router/main.cpp)
add_distplatform_executable(file_service services/file/main.cpp)
add_distplatform_executable(gateway_service services/gateway/main.cpp)

if(DIST_PLATFORM_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(unit_frame_codec tests/unit/test_frame_codec.cpp)
    target_link_libraries(unit_frame_codec PRIVATE distplatform_common GTest::gtest_main)
    add_test(NAME unit_frame_codec COMMAND unit_frame_codec)

    add_executable(unit_router_logic tests/unit/test_router_logic.cpp)
    target_link_libraries(unit_router_logic PRIVATE distplatform_common GTest::gtest_main)
    add_test(NAME unit_router_logic COMMAND unit_router_logic)

    add_executable(integration_storage tests/integration/test_storage.cpp)
    target_link_libraries(integration_storage PRIVATE distplatform_common GTest::gtest_main)
    add_test(NAME integration_storage COMMAND integration_storage)
endif()