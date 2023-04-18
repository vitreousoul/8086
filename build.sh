#!/usr/bin/env sh

DEBUG=1
SOURCE_FILES="src/sim.c"
SETTINGS="-std=c89 -Wall -Wextra -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Wno-comment"
OUTPUT_DIR="dist"

if [ ! -d "$OUTPUT_DIR" ]; then
    mkdir $OUTPUT_DIR
fi

if [ $DEBUG -eq 0 ]; then
    echo "Optimized build";
    TARGET="-O2 -o $OUTPUT_DIR/sim.exe"
elif [ $DEBUG -eq 1 ]; then
    echo "Debug build";
    TARGET="-g3 -O0 -o $OUTPUT_DIR/a.out"
fi

echo $TARGET
echo $SETTINGS
echo $SOURCE_FILES

gcc $TARGET $SETTINGS $SOURCE_FILES
