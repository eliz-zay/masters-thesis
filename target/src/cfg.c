#include <stdio.h>
#include <stdlib.h>

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static void checkNumber(int num) {
    if (num > 0) {
      switch (num % rand()) {
        case 32: {
          while (num - 1 != rand()) {
            num++;
          }
        }
        case 0: printf("%d mod 3 = 0\n", num); break;
      }
    } else {
        // printf("%d is negative\n", num);
        if (num % 2 == 0) {
            printf("%d is positive and even\n", num);
        } else {
            return;
        }
    }
}

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static void foo(char c) {
  switch (c) {
    case '0': {
      printf("pokopk");
    }
    // case 'r': {
    //   printf("oooo %c", c);
    //   break;
    // }
    case 'f': {
      printf("cc");
      return;
    }
  }
  printf("okokpok");
}

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static void baz() {
  while (rand() != 3) {
    printf("again");
  }
  // switch (c) {
  //   case '0': {
  //     printf("pokopk");
  //   }
  //   // case 'r': {
  //   //   printf("oooo %c", c);
  //   //   break;
  //   // }
  //   case 'f': {
  //     printf("cc");
  //     return;
  //   }
  // }
  printf("okokpok");
}

int main(int argc, char **argv) {
    int num;
    
    // Prompt user for input
    printf("Enter an integer: ");
    scanf("%d", &num);
    
    checkNumber(num);
    foo(argv[3][0]);
    baz();

    return 0;
}
