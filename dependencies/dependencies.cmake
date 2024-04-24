if("${DEPENDENCIES_PREFIX_INSTALL}" STREQUAL "")
    set(DEPENDENCIES_PREFIX_INSTALL ${CMAKE_CURRENT_BINARY_DIR}/dependencies)
endif()

set(dependencies_LIST)

include(ExternalProject)
ExternalProject_Add(libxutils_external
        GIT_REPOSITORY https://github.com/kala13x/libxutils
        GIT_TAG feature/fixes-for-cmake-added-macos-support
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${DEPENDENCIES_PREFIX_INSTALL} -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS})
list(APPEND dependencies_LIST libxutils_external)

add_custom_target(dependencies DEPENDS ${dependencies_LIST})
