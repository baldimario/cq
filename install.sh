#!/bin/bash
# Installation script for cq on Unix-like systems (Linux, macOS, BSD)
# Does not require make - compiles directly with cc/gcc

set -e  # Exit on error

echo "Installing cq..."
echo

# Detect OS and compiler
if command -v cc &> /dev/null; then
    CC=cc
elif command -v gcc &> /dev/null; then
    CC=gcc
elif command -v clang &> /dev/null; then
    CC=clang
else
    echo "Error: No C compiler found (cc, gcc, or clang)"
    exit 1
fi

echo "Using compiler: $CC"

# Compiler flags
CFLAGS="-Wall -W -O2 -Iinclude"

# Create directories
echo "Creating build directories..."
mkdir -p obj
mkdir -p build

# Compile source files
echo "Compiling csv_reader.c..."
$CC $CFLAGS -c src/csv_reader.c -o obj/csv_reader.o

echo "Compiling evaluator.c..."
$CC $CFLAGS -c src/evaluator.c -o obj/evaluator.o

echo "Compiling main.c..."
$CC $CFLAGS -c src/main.c -o obj/main.o

echo "Compiling parser.c..."
$CC $CFLAGS -c src/parser.c -o obj/parser.o

echo "Compiling tokenizer.c..."
$CC $CFLAGS -c src/tokenizer.c -o obj/tokenizer.o

echo "Compiling utils.c..."
$CC $CFLAGS -c src/utils.c -o obj/utils.o

# Link executable
echo "Linking cq..."
$CC obj/*.o -o build/cq

echo
echo "Build successful! Binary: build/cq"
echo
echo "You can now run: ./build/cq -h"
echo

# Detect architecture
if command -v file &> /dev/null; then
    echo "Binary info:"
    file build/cq
    
    # Show architecture on macOS
    if [[ "$OSTYPE" == "darwin"* ]] && command -v lipo &> /dev/null; then
        lipo -info build/cq
    fi
fi
