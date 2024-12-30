#include <stdio.h>

__attribute__((annotate("nookie:42")))
void foo() {

}

char __attribute__((annotate("nookie:43"))) theNookie = 'a';

__attribute__((annotate("flatten")))
int main() {
    foo();
    printf("Hello, LLVM!\n");

    // Local annotations are ignored in the pass
    int __attribute__((annotate("nookie:44"))) theNookieLocal = 100;

    return 0;
}
