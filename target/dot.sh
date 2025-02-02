#!/bin/bash
../../llvm-project/build/bin/opt \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/annotation/libAnnotationPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/flatten/libFlattenPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/bogus-switch/libBogusSwitchPass.so" \
  -load-pass-plugin="/Users/elizaveta/Documents/uni/thesis/thesis/pass/build/function-merge/libFunctionMergePass.so" \
  -passes="module(annotation),function(flatten),function(bogus-switch),module(function-merge)" \
  -o hello2.ll -S \
  hello.ll 
../../llvm-project/build/bin/opt -passes=dot-cfg hello2.ll -o /dev/null
dot -Tpng .main.dot -o main2.png
dot -Tpng .foo.dot -o foo2.png