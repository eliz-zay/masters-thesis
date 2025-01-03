#!/bin/bash
../../llvm-project/build/bin/opt \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/annotation/libAnnotationPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/skeleton/libSkeletonPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/flatten/libFlattenPass.so" \
  -passes="module(annotation),function(skeleton),function(flatten)" \
  -o hello2.ll -S \
  hello.ll 
../../llvm-project/build/bin/opt -passes=dot-cfg hello2.ll
dot -Tpng .main.dot -o main2.png