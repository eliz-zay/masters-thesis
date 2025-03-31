#include <stdio.h>
#include <stdlib.h>

static int foo(int n) {
  if (n < 107) {
    return 4;
  }

  return n;
}

static void bar(int n) {
  printf("%i", n);
}

__attribute__((noinline))
__attribute__((annotate("flatten")))
__attribute__((annotate("bogus-switch")))
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

  // printf("%i", n);

  bar(n);
}

int main(int argc, char *argv[]) {
    int n;
    scanf("%d", &n);
    baz(n, "b");
    return 0;
}