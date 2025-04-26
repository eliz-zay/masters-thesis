../../llvm-project/build/bin/llc --filetype=obj exec/orig.ll -o exec/orig.o
../../llvm-project/build/bin/clang \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk \
  exec/orig.o -o exec/orig