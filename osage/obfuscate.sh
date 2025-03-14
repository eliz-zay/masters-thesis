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

echo -e "${BLUE}Compiling...${NC}"

# Compile
/opt/llvm-project/build/bin/clang \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
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
  -passes="module(annotation),function(flatten),function(bogus-switch),module(function-merge)" \
  -o build/obf.ll -S \
  build/orig.ll

echo -e "${BLUE}Compiling IR to binary...${NC}"

# Convert IR file to a binary
/opt/llvm-project/build/bin/clang -static build/obf.ll -o "$OUT_FILE"

if [ $? -eq 0 ]; then
  echo -e "${BLUE}Executable created: $OUT_FILE${NC}"
else
  echo -e "${BLUE}Compilation failed${NC}"
  exit 1
fi
