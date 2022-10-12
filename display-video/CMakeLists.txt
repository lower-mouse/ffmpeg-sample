# cmake_minimum_required(VERSION 3.4.1)
# project(demo1)
# file(GLOB SRC_LIST "*.cpp")
# target_link_libraries (demo1
# avformat avcodec swresample avutil)
# # include_directories(
# #     ${CMAKE_CURRENT_BINARY_DIR}
# #     ${CMAKE_CURRENT_SOURCE_DIR}/include
# # )
# # target_link_libraries(demo avformat avcodec swresample avutil pthread)
# add_executable(demo ${SRC_LIST}) # 生成可执行文件

# #add_library(common STATIC util.cpp) # 生成静态库
# #add_library(common SHARED util.cpp) # 生成动态库或共享库

cmake_minimum_required(VERSION 3.4.1)

project(demo)
file(GLOB SRC_FILE "*.c")
add_executable(demo ${SRC_FILE})
target_link_libraries(demo -lGL -lGLU -lglut avformat avcodec swresample swscale avutil -llzma  -lbz2 -lz  -lm pthread)
