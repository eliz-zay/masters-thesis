#!/bin/bash
# Vizualize CFG
../../llvm-project/build/bin/opt -passes=dot-cfg exec/orig.ll
dot -Tpng .square_array.dot -o out/square_array.orig.png
dot -Tpng .main.dot -o out/main.orig.png
rm .*.dot