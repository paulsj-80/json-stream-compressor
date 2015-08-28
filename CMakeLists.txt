cmake_minimum_required(VERSION 2.8.9)
project(json-stream-compressor)
set (CMAKE_CXX_FLAGS "-std=c++11")
set(CMAKE_BUILD_TYPE Debug)

find_package(Boost 1.54 REQUIRED)

include_directories(include lib/fast-json-parser/dist/include)

add_library(json_compressor STATIC src/json_compressor.cpp)

link_directories(lib/fast-json-parser/dist/lib)
add_executable(test test/test.cpp)
target_link_libraries(test json_compressor jp_wrapper JSON_checker)

