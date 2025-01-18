#!/bin/bash
../../llvm-project/build/bin/opt -passes=dot-cfg hello.ll
dot -Tpng .main.dot -o main.png