#!/bin/bash
/opt/llvm-project/build/bin/clang \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  -emit-llvm -O3 -S \
  -o exec/orig.ll \
  $1