#!/bin/bash

CC=clang
CFLAGS="-Wall -I./argp-standalone/include -L./arg-standalone/build/src"
OUT_DIR=bin
SRC_DIR=src
NAME=share
ARGP_DIR="argp-standalone"

if [[ "$1" == "prod" ]]; then
    echo "production build"
    CFLAGS="$CFLAGS -O3"
else
    CFLAGS="$CFLAGS -g"
fi

if [ ! -d "$OUT_DIR" ]; then
    (set -x; mkdir "$OUT_DIR")
fi

if [ ! -d "$ARGP_DIR/build" ]; then
    (set -x; mkdir "$ARGP_DIR/build")
fi

(set -xe; cd "$ARGP_DIR/build" && cmake -DCMAKE_BUILD_TYPE=Release ..)
(set -xe; cd "$ARGP_DIR/build" && make)

if [ ! -d "$SRC_DIR" ]; then
    echo "src director ($SRC_DIR) not found"
    exit 1
fi

(set -xe; $CC $CFLAGS "$SRC_DIR/server.c" "$SRC_DIR/main.c" -o "$OUT_DIR/$NAME")