cmake_minimum_required(VERSION 3.10)

project(xmedia)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall -D_XUTILS_DEBUG")

include_directories(${PROJECT_SOURCE_DIR}/src)

set(SOURCES
  ${PROJECT_SOURCE_DIR}/src/codec.c
  ${PROJECT_SOURCE_DIR}/src/decoder.c
  ${PROJECT_SOURCE_DIR}/src/encoder.c
  ${PROJECT_SOURCE_DIR}/src/frame.c
  ${PROJECT_SOURCE_DIR}/src/meta.c
  ${PROJECT_SOURCE_DIR}/src/mpegts.c
  ${PROJECT_SOURCE_DIR}/src/nalu.c
  ${PROJECT_SOURCE_DIR}/src/status.c
  ${PROJECT_SOURCE_DIR}/src/stream.c
  ${PROJECT_SOURCE_DIR}/src/version.c
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    xutils
    pthread
    avutil
    avcodec
    avformat
    avdevice
    swscale
    swresample
)

install(TARGETS ${PROJECT_NAME} ARCHIVE DESTINATION lib)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION xmedia FILES_MATCHING PATTERN "*.h")
