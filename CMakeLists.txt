cmake_minimum_required(VERSION 3.28)

project(r_type VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(r-type_client client/src/main.cpp)
add_executable(r-type_server server/src/main.cpp)

enable_testing()
add_subdirectory(tests)
