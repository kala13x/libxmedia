cmake_minimum_required(VERSION 3.10)

project(xmedia)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall -D_XUTILS_DEBUG")

set(SOURCE_FILES
    xmedia.c
)

set(XMEDIA_LIBRARY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../build)
set(XMEDIA_STATIC_LIB ${XMEDIA_LIBRARY_ROOT}/libxmedia.a)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${XMEDIA_LIBRARY_ROOT}/../src)

# Linking static and shared libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${XMEDIA_STATIC_LIB}
        xutils
        pthread
        avutil
        avcodec
        avformat
        avdevice
        swscale
        swresample
        freetype
)

# Installation rules
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION /usr/local/bin
)

# Clean up build directory
set_property(DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_BINARY_DIR}/build"
)
