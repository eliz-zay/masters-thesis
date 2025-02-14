#include<stdio.h>
#include <stdlib.h>
int r,s;

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static int fact(int n){
  if(n==1)
    return 1;
  else
    return(n*fact(n-1));
}

__attribute__((noinline))
__attribute__((annotate("function-merge")))
static int factorialrec(int argc, char* argv[]) {
  if (argc < 2) return 1;
  int num,f;
  num = atoi(argv[1]);
  f=fact(num);
  printf("Factorial: %d", f);
  return 0;
}

int main(int argc, char* argv[]){
  factorialrec(argc, argv);
  return 0;
}