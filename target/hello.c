#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static int foo(int n) {
  switch (n) {
      case 11: 
          n = n % 11;
          break;
      
      case 22: 
          n = n % 202;
          break;
      
      case 33: {n = 999; break;}
      case 44: {n = rand(); break;}
      default: {n = 888; break;}
  }

  return n;
}

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static void bar(int n) {
  printf("%i", n);
}

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static void baz(int n, char *c) {
  switch (n) {
      case 11: 
          n = n % 11;
          break;
      
      case 22: 
          n = n % 202;
          break;
      
      case 33: {n = 999; break;}
      case 44: {n = rand(); break;}
      default: {n = 888; break;}
  }

  bar(n);
}

int main(int argc, char *argv[]) {
    int n = rand();
    int res1 = foo(n);
    baz(res1, "b");
    return foo(res1);
}