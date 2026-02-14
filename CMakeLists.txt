cmake_minimum_required(VERSION 3.16)
project(placement-backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Drogon CONFIG REQUIRED)
find_package(mongocxx CONFIG REQUIRED)
find_package(bsoncxx CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

# Collect sources
file(GLOB_RECURSE SOURCES
    "main.cc"
    "controllers/*.cc"
    "services/*.cc"
    "utils/*.cc"
    "middleware/*.cc"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Prevent Windows min/max macros from conflicting with C++ standard library
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/middleware
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Drogon::Drogon
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
    OpenSSL::SSL
    OpenSSL::Crypto
)
