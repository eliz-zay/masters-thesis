# Thesis

## OSAGE and Docker
- `docker build -t thesis-v20-x86 -f Dockerfile.x86 .`
- `OSAGE_DIR=<full_path_to_OSAGE>` - host path to OSAGE directory   
- ```shell
    docker run \
      -v $OSAGE_DIR/<path_to_src_file.c>:/app/in/target.c \
      -v $OSAGE_DIR/<path_to_output_dir>:/app/out \
      -it thesis-v20 \
      <obfuscation_modes> <output_file.out>
  ```
  Target C file is copied from host to `/app/in/target.c` inside a container. Host output directory is linked
  to `/app/out` inside a Docker container, and **a binary is generated in <path_to_output_dir>/<output_file.out>**.
  For example, this command obfuscates `anagram.c` and generates `my-obfuscator/out.out`:
  ```shell
    docker run \
      -v $OSAGE_DIR/src_strings/anagram/anagram.c:/app/in/target.c \
      -v $OSAGE_DIR/my-obfuscator:/app/out \
      -it thesis-v20-x86 \
      'flatten:checkAnagram' out.out
  ```

### Obfuscation names
- `flatten` - Control-Flow-Flattening
- `bogus-switch` - bogus control flow for `switch` statements. *It complements Control-Flow-Flattening. Use either `flatten`, or `flatten` and then `bogus-switch`.* This obfuscation is useless when used without `flatten`
- `function-merge` - function merging, specify for multiple functions at once
- `mba` - instruction equivalence, Mixed Boolean-Arithmetic expressions

Usage examples with functions `foo`, `bar`, `baz`:
- `flatten:foo,bar`
- `flatten:foo,bar,baz;bogus-switch:foo,bar;mba:foo,bar`
- `function-merge:foo,bar,baz`
- `flatten:foo;bogus-switch:foo;function-merge:foo,bar,baz;mba:foo,bar,baz`

### Inside Docker container

- `./docker/validate.sh <input list>` - executes both original and obfuscated version a given input and compares the result

## TODO

> учитывать последовательность пассов (?)   
> вынести получение annotation и удаление фи нод в либу   

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

To forward output to a new file, replace `-disable-output` with `-o hello2.ll`.

`file <filename>`, `file ./bin/opt`

## Visualize CFG of a function
Prerequisite: `brew install graphviz`

- `opt -passes=dot-cfg hello.ll` - generate `.main.dot`
- `dot -Tpng .main.dot -o main.png` - generate `main.png`
Works for every function in the program.

## Validation
- [FFT](https://lloydrochester.com/post/c/example-fft/)
- [AES](https://github.com/kokke/tiny-AES-c)
- [SHA 256](https://github.com/EddieEldridge/SHA256-in-C)
- [RSA (cinnabar)](https://github.com/PascalLG/cinnabar-c)
