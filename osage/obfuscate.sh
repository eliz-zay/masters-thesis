#!/bin/bash
set -e

BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ "$#" -ne 2 ]; then
  echo -e "${BLUE}Usage: $0 <source_file.c> <output_file.out>${NC}"
  exit 1
fi

SRC_FILE=$1
OUT_FILE=$2

x86_SYSROOT_DIR=/opt/sysroot-x86_64-linux-gnu

echo -e "${BLUE}Compiling...${NC}"

# Compile
# zig cc \
#   -target x86_64-linux-gnu \
#   -emit-llvm -O3 -S \
#   -o build/orig.ll \
#   "$SRC_FILE"
/opt/llvm-project/build/bin/clang \
  --target=x86_64-linux-gnu \
  --sysroot=$x86_SYSROOT_DIR \
  -emit-llvm -O3 -S \
  -o build/orig.ll \
  "$SRC_FILE"

echo -e "${BLUE}Obfuscating...${NC}"

# Apply obfuscations using optimizer
/opt/llvm-project/build/bin/opt \
  -load-pass-plugin="/app/pass/build/annotation/libAnnotationPass.so" \
  -load-pass-plugin="/app/pass/build/flatten/libFlattenPass.so" \
  -load-pass-plugin="/app/pass/build/bogus-switch/libBogusSwitchPass.so" \
  -load-pass-plugin="/app/pass/build/function-merge/libFunctionMergePass.so" \
  -load-pass-plugin="/app/pass/build/mba/libMBAPass.so" \
  -passes="module(annotation),function(flatten),function(bogus-switch),module(function-merge),function(mba)" \
  -o build/obf.ll -S \
  build/orig.ll

echo -e "${BLUE}Compiling IR to binary...${NC}"

# Convert IR file to a binary
# zig cc -target x86_64-linux-gnu build/obf.ll -o "$OUT_FILE"
/opt/llvm-project/build/bin/clang \
  --target=x86_64-linux-gnu \
  build/obf.ll -o "$OUT_FILE"

if [ $? -eq 0 ]; then
  echo -e "${BLUE}Executable created: $OUT_FILE${NC}"
else
  echo -e "${BLUE}Compilation failed${NC}"
  exit 1
fi
