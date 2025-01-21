#!/bin/bash
../../llvm-project/build/bin/opt \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/annotation/libAnnotationPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/skeleton/libSkeletonPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/flatten/libFlattenPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/bogus-switch/libBogusSwitchPass.so" \
  -passes="module(annotation),function(flatten),function(bogus-switch)" \
  -o hello2.ll -S \
  hello.ll 
../../llvm-project/build/bin/opt -passes=dot-cfg hello2.ll
dot -Tpng .main.dot -o main2.png