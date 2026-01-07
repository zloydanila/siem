cmake_minimum_required(VERSION 3.10)
project(NoSQL_DB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json QUIET)

add_library(db_core
    containers/vector.cpp
    containers/hash_map.cpp
    containers/unordered_set.cpp
    containers/queue.cpp
    schema/schema.cpp
    database/database.cpp
    collection/collection.cpp
)

target_include_directories(db_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/containers
)

add_executable(mydb
    main.cpp
)
target_link_libraries(mydb PRIVATE db_core)

add_executable(db_server
    network/server.cpp
)
target_link_libraries(db_server PRIVATE db_core)

add_executable(db_client
    network/client.cpp
)
target_include_directories(db_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(db_client PRIVATE db_core)

if(nlohmann_json_FOUND)
    target_link_libraries(db_core PUBLIC nlohmann_json::nlohmann_json)
else()
    message(WARNING "nlohmann_json не найден. Установите: sudo apt install nlohmann-json3-dev")
endif()
