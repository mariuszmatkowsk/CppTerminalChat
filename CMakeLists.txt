cmake_minimum_required(VERSION 3.31)

project(chat)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(ASIO_USER_DEFINED_PATH "/usr/include" CACHE PATH "Path to ASIO include directory")
add_library(asio INTERFACE)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
target_include_directories(asio INTERFACE ${ASIO_USER_DEFINED_PATH})

add_subdirectory(src)
