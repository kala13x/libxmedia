cmake_minimum_required(VERSION 3.10)
project(xmedia)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O2 -Wall -D_XUTILS_DEBUG")

option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)

include(dependencies/dependencies.cmake)

file(GLOB SOURCES src/*.c)
add_library(${PROJECT_NAME}
    ${SOURCES})

add_dependencies(${PROJECT_NAME} dependencies)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${DEPENDENCIES_PREFIX_INSTALL}/include
    ${DEPENDENCIES_PREFIX_INSTALL}/include/xutils # TODO: temporary inconvinient should be reduced usage "" in the xutils headers
    src)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavutil
    libavcodec
    libavformat
    libavdevice
    #libavfilter
    libswscale
    libswresample)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    -L${DEPENDENCIES_PREFIX_INSTALL}/lib # TODO: expected all dependencies located here
    xutils
    pthread
    PkgConfig::LIBAV)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR})

install(DIRECTORY src/ DESTINATION  ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/xmedia FILES_MATCHING PATTERN "*.h")
