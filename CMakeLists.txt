cmake_minimum_required(VERSION 3.0)
project(muduo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加-g选项，启用调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# 添加头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/http/include)
include_directories(${PROJECT_SOURCE_DIR}/redis)

# 添加 hiredis 头文件目录
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)

# 查找所有源文件
file(GLOB_RECURSE SOURCES
    ${PROJECT_SOURCE_DIR}/src/*.cc
    ${PROJECT_SOURCE_DIR}/http/src/*.cc
    ${PROJECT_SOURCE_DIR}/redis/*.cc
)
message(STATUS "Found sources: ${SOURCES}")  # 调试用，可选

# 创建静态库
add_library(muduo ${SOURCES})

# 添加 HttpServer_test 可执行文件
add_executable(HttpServer_test ${PROJECT_SOURCE_DIR}/http/HttpServer_test.cc)
# 链接 HttpServer_test 可执行程序到 muduo 库
target_link_libraries(HttpServer_test
    muduo
    ${HIREDIS_LIBRARY}
    pthread
)