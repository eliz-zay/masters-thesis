#!/bin/bash

BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ "$#" -ne 2 ]; then
  echo -e "${BLUE}Usage: $0 <source_file.c> <output_file.out>${NC}"
  exit 1
fi

SRC_FILE=$1
OUT_FILE=$2

# Compile
/opt/llvm-project/build/bin/clang \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  -emit-llvm -O3 -S \
  -o exec/orig.ll \
  "$SRC_FILE"

# Apply obfuscations using optimizer
/opt/llvm-project/build/bin/opt \
  -load-pass-plugin="/app/pass/build/annotation/libAnnotationPass.so" \
  -load-pass-plugin="/app/pass/build/flatten/libFlattenPass.so" \
  -load-pass-plugin="/app/pass/build/bogus-switch/libBogusSwitchPass.so" \
  -load-pass-plugin="/app/pass/build/function-merge/libFunctionMergePass.so" \
  -passes="module(annotation),function(flatten),function(bogus-switch),module(function-merge)" \
  -o exec/obf.ll -S \
  exec/orig.ll

# Convert IR file to a binary
/opt/llvm-project/build/bin/clang exec/obf.ll -o "$OUT_FILE"

if [ $? -eq 0 ]; then
  echo -e "${BLUE}Executable created: $OUT_FILE${NC}"
else
  echo -e "${BLUE}Compilation failed${NC}"
  exit 1
fi
