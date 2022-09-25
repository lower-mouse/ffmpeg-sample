
cmake_minimum_required(VERSION 3.4.1)

project(demo)
file(GLOB SRC_FILE "*.c")
add_executable(demo ${SRC_FILE})
target_link_libraries(demo -lGL -lGLU -lglut)