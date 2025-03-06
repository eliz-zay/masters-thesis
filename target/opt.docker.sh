#!/bin/bash
# Apply obfuscations using optimizer
/opt/llvm-project/build/bin/opt \
  -load-pass-plugin="../pass/build/annotation/libAnnotationPass.so" \
  -load-pass-plugin="../pass/build/flatten/libFlattenPass.so" \
  -load-pass-plugin="../pass/build/bogus-switch/libBogusSwitchPass.so" \
  -load-pass-plugin="../pass/build/function-merge/libFunctionMergePass.so" \
  -passes="module(annotation),function(flatten),function(bogus-switch),module(function-merge)" \
  -o exec/obf.ll -S \
  exec/orig.ll
