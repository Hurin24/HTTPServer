cmake_minimum_required(VERSION 3.5)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(HTTPServer)

set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

set(CMAKE_CXX_STANDARD 17)

#asio
add_library(asio INTERFACE)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/libs/asio/asio/include)

#Simple-Web-Server
set(USE_STANDALONE_ASIO ON)
set(BUILD_TESTING OFF)
list(APPEND CMAKE_REQUIRED_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/libs/asio/asio/include/asio.hpp")

message(${CMAKE_REQUIRED_INCLUDES})
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/asio/asio/include)
add_subdirectory(libs/Simple-Web-Server)

add_executable(HTTPServer src/main.cpp)
