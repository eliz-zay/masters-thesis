# Thesis

## Docker
- `docker build -t thesis-v20 .`
- `docker run -it thesis-v20 /bin/bash`

### Inside Docker container

- `./docker/gen-binary.sh <C file> <output file>` - obfuscate and generate a binary
- `./docker/validate.sh <input list>` - executes both original and obfuscated version a given input and compares the result

- Add dockerfile entry point which accepts C file and produces executables (llc?)
- Add a script which parses docker input, copies source file, add the annotations to C file based on the input, and then feeds it to the obfuscator
`docker run -it thesis-v20 <source path> <output path> "flatten:KeyExpansion,main;bogus-switch:KeyExpansion"`

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
- `./bin/llvm-config  --version` - check LLVM version

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
- `cd ./thesis/pass/build`
- `LLVM_HOME=/Users/elizaveta/Documents/uni/thesis/llvm-project/build cmake ..`
- `make`

### Run
- `cd ./thesis/target`
- ```shell
  opt \
    -disable-output \
    -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/annotation/libAnnotationPass.so" \
    -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/skeleton/libSkeletonPass.so" \
    -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/flatten/libFlattenPass.so" \
    -passes="module(annotation),function(skeleton),function(flatten)" \
    hello.ll
  ```

To forward output to a new file, replace `-disable-output` with `-o hello2.ll`.

`file <filename>`, `file ./bin/opt`

## Visualize CFG of a function
Prerequisite: `brew install graphviz`

- `opt -passes=dot-cfg hello.ll` - generate `.main.dot`
- `dot -Tpng .main.dot -o main.png` - generate `main.png`
Works for every function in the program.

## Implementation
LLVM IR does not keep annotations attached to functions and variables directly. Instead, the annotations are kept in the global section
and cannot be accessed directly from values. To solve this, the Annotation module pass iterates over all annotations in the module
modifies the IR by attaching 'annotation' metadata directly to the annotated values. The subsequent function passes can access this data
directly from metadata. Important note: the annotation pass does not modify the IR .ll file unless explicitly specified to, but it modifies
the intermediate representation of the code shared between passes.

## Validation
- [FFT](https://lloydrochester.com/post/c/example-fft/)
- [AES](https://github.com/kokke/tiny-AES-c)
- [SHA 256](https://github.com/EddieEldridge/SHA256-in-C)
- [RSA (cinnabar)](https://github.com/PascalLG/cinnabar-c)
