#!/bin/bash

set -xe

[ -d ./build ] && rm -rf ./build
mkdir -p ./build

CC=clang
CFLAGS="-std=c99 -O3 -march=native -Wall -Wextra -Werror"
LDLIBS="-lm -lraylib -lfftw3 -lpthread"

SRC_FILES=(
  "./src/main.c"
)
INCLUDE_DIRS="-I./include"
OUTPUT_DIR="./build"
EXECUTABLE_NAME="c_fft_visualizer"

cp -r assets $OUTPUT_DIR

$CC $CFLAGS $INCLUDE_DIRS -o $OUTPUT_DIR/$EXECUTABLE_NAME ${SRC_FILES[@]} $LDLIBS
