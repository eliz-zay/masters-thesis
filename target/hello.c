#include <stdio.h>

__attribute__((annotate("nookie:42")))
void foo() {

}

char __attribute__((annotate("nookie:43"))) theNookie = 'a';

__attribute__((annotate("flatten")))
int main(int argc) {
    int n = argc;
    
    while (n > 4) {
        if (n % 2 == 0) {
            n = n / 2;
        } else {
            n = n * 3 + 1;
        }

        while (n % 3 == 0) {
            n = n - 1;
        }


        switch (n) {
            case 77: {
                while (n > 17) {
                    n = n - 5;
                }
                break;
            }
            case 22: {
                while (n > 4) {
                    if (n % 2 == 0) {
                        n = n / 2;
                    } else {
                        n = n * 3 + 1;
                    }
                }
                break;
            }
            case 44: {
                n = 5;
                break;
            }
        }
    }

    return n;
}
