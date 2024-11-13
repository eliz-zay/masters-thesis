#include <stdio.h>

void foo() {

}

int main() {
    foo();
    printf("Hello, LLVM!\n");

    int __attribute__((annotate("nookie:42"))) theNookie = 100;

    return 0;
}
