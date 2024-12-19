## Install llvm, build clang and lli

https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm

- `git clone https://github.com/llvm/llvm-project.git`
- `cd llvm-project`
- `mkdir build`
- `cd build`
- ```shell
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


## Compiling

[New llvm pass manager](https://llvm.org/docs/NewPassManager.html)

### Setup

- `mkdir ./thesis/target` and create `./thesis/target/hello.c`
- `cd ./thesis/target`

> _clang_ and _opt_ are located in ../../llvm-project/build/bin

- `clang -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -emit-llvm -O3 -S hello.c` - compile to `.ll`. `-O3` flag indicates that LLVM optimizer can perform optimizations and transformations
- `opt -disable-output hello.ll -passes=helloworld` - perform passes using optimizer


## Passes

### Build the pass
- `cd ./pass/build`
- `LLVM_HOME=/Users/elizaveta/Documents/uni/thesis/llvm-project/build cmake ..`
- `make`

### Run
- `cd ./thesis/target`
- ```shell
  opt \
    -disable-output \
    -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/annotation/libAnnotationPass.so" \
    -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/skeleton/libSkeletonPass.so" \
    -passes="module(annotation),function(skeleton)" \
    hello.ll
```

`file <filename>`, `file ./bin/opt`
