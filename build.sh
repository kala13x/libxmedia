#!/bin/bash
# This source is part of "libxmedia" project
# 2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)

PROJ_PATH=$(dirname $(readlink -f "$0"))
EXAMPLE_PATH=$PROJ_PATH/tools
LIBRARY_PATH=$PROJ_PATH

MAKE_TOOL="make"
EXAMPLE_DONE=0
LIBRARY_DONE=0
CPU_COUNT=1

for arg in "$@"; do
    if [[ $arg == --tool=* ]]; then
        MAKE_TOOL="${arg#*=}"
        echo "Using tool: $MAKE_TOOL"
    fi
done

update_cpu_count() {
    if [ $OSTYPE == linux-gnu ]; then
        CPU_COUNT=$(nproc)
    fi
}

prepare_build() {
    if [[ $MAKE_TOOL == "cmake" ]]; then
        if [[ -f $PROJ_PATH/misc/cmake.tmp ]]; then
            cp $PROJ_PATH/misc/cmake.tmp $PROJ_PATH/CMakeLists.txt
        else
            echo "CMakeLists.txt template file is not found!"
            exit 1;
        fi
    elif [[ $MAKE_TOOL == "smake" ]]; then
        if [[ -f $PROJ_PATH/misc/smake.tmp ]]; then
            cp $PROJ_PATH/misc/smake.tmp $PROJ_PATH/smake.json
        else
            echo "SMake template file is not found!"
            exit 1;
        fi
    elif [[ $MAKE_TOOL == "make" ]]; then
        if [[ -f $PROJ_PATH/misc/make.tmp ]]; then
            cp $PROJ_PATH/misc/make.tmp $PROJ_PATH/Makefile
        else
            echo "Makefile template is not found!"
            exit 1;
        fi
    else
        echo "Unknown build tool: $MAKE_TOOL"
        echo "Specify cmake, smake or make)"
        exit 1
    fi
}

build_example() {
    [ "$EXAMPLE_DONE" -eq 1 ] && return
    cd $PROJ_PATH/example

    if [[ $MAKE_TOOL == "cmake" ]]; then
        mkdir -p build && cd build
        cmake .. && make -j $CPU_COUNT
        EXAMPLE_PATH=$PROJ_PATH/example/build
    elif [[ $MAKE_TOOL == "smake" ]]; then
        smake && make -j $CPU_COUNT
        EXAMPLE_PATH=$PROJ_PATH/example
    elif [[ $MAKE_TOOL == "make" ]]; then
        make -j $CPU_COUNT
        EXAMPLE_PATH=$PROJ_PATH/example
    fi

    EXAMPLE_DONE=1
}

build_lbrary() {
    [ "$LIBRARY_DONE" -eq 1 ] && return
    cd $PROJ_PATH

    if [[ $MAKE_TOOL == "cmake" ]]; then
        mkdir -p build && cd build
        cmake .. && make -j $CPU_COUNT
        LIBRARY_PATH=$PROJ_PATH/build
    elif [[ $MAKE_TOOL == "smake" ]]; then
        smake && make -j $CPU_COUNT
        LIBRARY_PATH=$PROJ_PATH
    elif [[ $MAKE_TOOL == "make" ]]; then
        make -j $CPU_COUNT
        LIBRARY_PATH=$PROJ_PATH
    fi

    LIBRARY_DONE=1
}

install_library() {
    build_lbrary
    cd $LIBRARY_PATH
    sudo make install
}

install_example() {
    build_example
    cd $EXAMPLE_PATH
    sudo make install
}

clean_path() {
    cd "$@" 
    [ -f ./Makefile ] && make clean
    [ -d ./obj ] && rm -rf ./obj
    [ -d ./build ] && rm -rf ./build
    [ -d ./CMakeFiles ] && rm -rf ./CMakeFiles
    [ -f ./CMakeCache.txt ] && rm -rf ./CMakeCache.txt
    [ -f ./cmake_install.cmake ] && rm -rf ./cmake_install.cmake
    [ -f ./install_manifest.txt ] && rm -rf ./install_manifest.txt
    cd $PROJ_PATH
}

clean_project() {
    clean_path $PROJ_PATH
    clean_path $PROJ_PATH/example
}

update_cpu_count
prepare_build
clean_project
build_lbrary

for arg in "$@"; do
    if [[ $arg == "--example" || $arg == "-e" ]]; then
        build_example
    fi

    if [[ $arg == "--install" || $arg == "-i" ]]; then
        install_library
        install_example
    fi
done

# Do cleanup last
for arg in "$@"; do
    if [[ $arg == "--clean" || $arg == "-c" ]]; then
        clean_project
    fi
done

exit 0
