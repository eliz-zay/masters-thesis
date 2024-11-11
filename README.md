## Install llvm, build clang and lli

https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm

- `git clone https://github.com/llvm/llvm-project.git`
- `cd llvm-project`
- `mkdir build`
- `cd build`
- 
  ```
  cmake \
    -G 'Unix Makefiles' \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DEFAULT_TARGET_TRIPLE='aarch64-apple-darwin23.1.0' \
    -DLLVM_TARGETS_TO_BUILD=AArch64 \
    ../llvm
  ```
- `make` - build LLVM and `clang`
- `./bin/clang --version` - verify `clang`
- `./bin/lli --version` - verify `lli`

## Hello world: built-in pass

[New llvm pass manager](https://llvm.org/docs/NewPassManager.html)

### C => bitcode
- `./bin/clang -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -emit-llvm -O3 -c hello.c` - compile to `.bc` bitcode file.
- `./bin/clang -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -emit-llvm -O3 -S hello.c` - compile to `.ll`. `-O3` flag indicates that LLVM optimizer can perform optimizations and transformations
- `./bin/lli hello.bc` - execute
- `./bin/opt -disable-output hello.ll -passes=helloworld` - perform passes using optimizer

- `./bin/llvm-dis < hello.bc | less` - view LVM assembly code

### bitcode => assembly
- `./bin/llc hello.bc -o hello.s` - generate `hello.s` naive assembly file from LLVM bitcode
- `gcc hello.s -o hello.native` - assemble naive assenbly into executable
- `./hello.native` - execute naive code

## Skeleton pass: plugin

### Build the pass
- `cd ./pass/build`
- `LLVM_HOME=/Users/elizaveta/Documents/uni/thesis/llvm-project/build cmake ..`
- `make`

### Run
- `./bin/opt -disable-output -load-pass-plugin=/Users/elizaveta/Documents/uni/thesis/masters-thesis/pass/build/skeleton/libSkeletonPass.so hello.ll -passes=skeleton`

`file <filename>`, `file ./bin/opt`
