#!/bin/bash

CC=clang
CFLAGS="-Wall"
OUT_DIR=bin
SRC_DIR=src
NAME=share

if [[ "$1" == "prod" ]]; then
    echo "production build"
    CFLAGS="$CFLAGS -O3"
else
    CFLAGS="$CFLAGS -g"
fi

if [ ! -d "$OUT_DIR" ]; then
    (set -x; mkdir "$OUT_DIR")
fi

if [ ! -d "$SRC_DIR" ]; then
    echo "src director ($SRC_DIR) not found"
    exit 1
fi

(set -xe; $CC $CFLAGS "$SRC_DIR/server.c" "$SRC_DIR/main.c" -o "$OUT_DIR/$NAME")