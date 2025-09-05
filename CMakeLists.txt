## 测试 朴素版本 LRU Cache

cmake_minimum_required(VERSION 3.10)

project(myLruCacheTest)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 11)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_executable(myLruCacheTest ./test/myLruCacheTest.cpp)