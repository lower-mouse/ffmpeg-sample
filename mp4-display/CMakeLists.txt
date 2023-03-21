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

project(mp4-player)
file(GLOB SRC_FILE "*.cpp")

set(OpenCV_DIR /home/intellif/work/ffmpeg-sample/output/lib/cmake/opencv4)

find_package(OpenCV REQUIRED)
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../mp4v2/include
    /home/intellif/work/ffmpeg-sample/output/include
)

link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../mp4v2/lib
    )
    
add_executable(${PROJECT_NAME} ${SRC_FILE})
    
target_link_libraries(${PROJECT_NAME} jsoncpp mp4Reader mp4v2 avformat avcodec swresample swscale avutil -lm pthread)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS} )


set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR})
SET(FUNC_INSTALL_DIR ../../output)

INSTALL(TARGETS ${PROJECT_NAME}
		CONFIGURATIONS Release
		DESTINATION ${FUNC_INSTALL_DIR})

INSTALL(FILES README.md 
        CONFIGURATIONS Release        
        DESTINATION ${FUNC_INSTALL_DIR})
