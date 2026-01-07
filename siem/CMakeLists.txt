cmake_minimum_required(VERSION 3.10)
project(NoSQL_DB LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Библиотека
add_library(db_core
    containers/vector.cpp containers/hash_map.cpp containers/unordered_set.cpp 
    containers/queue.cpp schema/schema.cpp database/database.cpp 
    collection/collection.cpp
)
target_include_directories(db_core PUBLIC . include containers)

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(db_core PUBLIC nlohmann_json::nlohmann_json)
endif()

# DB SERVER (только server.cpp!)
add_executable(db_server network/server.cpp)
target_link_libraries(db_server PRIVATE db_core)

# Клиент (опционально)
add_executable(db_client network/client.cpp)
target_link_libraries(db_client PRIVATE db_core)
