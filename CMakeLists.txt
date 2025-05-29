cmake_minimum_required(VERSION 3.0)
project(muduo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加-g选项，启用调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# 添加头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)

# 查找所有源文件
file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cc)
message(STATUS "Found sources: ${SOURCES}")  # 调试用，可选

# 创建静态库
add_library(muduo ${SOURCES})

# 添加 test 可执行文件
add_executable(test ${PROJECT_SOURCE_DIR}/test/TimerQueue_test.cc)

# 链接 test 可执行程序到 muduo 库
target_link_libraries(test muduo)