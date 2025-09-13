## 测试 朴素版本 LRU Cache

cmake_minimum_required(VERSION 3.10)

project(testAllCachePolicy)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(CMAKE_CXX_STANDARD 20)

# 设置构建类型为Debug
set(CMAKE_BUILD_TYPE "Debug")

# 为Debug模式添加编译选项
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -O0 -Wall -g -ggdb")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_executable(${PROJECT_NAME} ./test/testAllCachePolicy.cpp)