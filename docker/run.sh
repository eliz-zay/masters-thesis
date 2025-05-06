#!/bin/bash
set -e

BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ "$#" -ne 1 ]; then
  echo -e "${BLUE}Usage: $0 <output_file.out>${NC}"
  exit 1
fi

SRC_FILE="/app/in/target.c"
OUT_FILE="/app/out/$1"

echo -e "${BLUE}Compiling...${NC}"

# Compile
zig cc \
  -target x86_64-linux-gnu \
  -emit-llvm -O3 -S \
  -g0 \
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
  -passes="module(annotation),module(function-merge),function(flatten),function(bogus-switch),function(mba)" \
  -o build/obf.ll -S \
  build/orig.ll

echo -e "${BLUE}Compiling IR to binary...${NC}"

# Convert IR file to a binary
zig cc -target x86_64-linux-gnu build/obf.ll -o "$OUT_FILE"

if [ $? -eq 0 ]; then
  echo -e "${BLUE}Executable created!${NC}"
else
  echo -e "${BLUE}Compilation failed${NC}"
  exit 1
fi

