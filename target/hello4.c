#include <stdio.h>
#include <stdlib.h>

__attribute__((annotate("flatten")))
__attribute__((annotate("bogus-switch")))
int foo(int n) {
    int n = atoi(argv[1]);

    while (1) {
        if (rand() % 2 < 1) {
            break;
        }
    }

    if (n > 0) {
        switch (n) {
            case 11: 
                n = n % 11;
                break;
            
            case 22: 
                n = n % 202;
                break;
            
            case 33: {n = 999; break;}
            default: {n = 888; break;}
        }
    }
    int iteration_limit = 1000; // Prevent infinite loops
    int iterations = 0;

    while (n != 0 && iterations < iteration_limit) {
        if (n % 2 == 0) {
            n += 5;
        } else {
            n -= 3;
        }

        while (n % 3 == 0 && iterations < iteration_limit) {
            n /= 3;
            iterations++;
        }

        switch (n % 4) {
            case 0:
                for (int i = 0; i < 2; ++i) {
                    n += i * 2;
                    iterations++;
                    if (iterations >= iteration_limit) break;
                }
                break;
            case 1:
                while (n % 5 != 0 && iterations < iteration_limit) {
                    n -= 1;
                    iterations++;
                }
                break;
            case 2:
                if (n < 50) {
                    n *= 2;
                } else {
                    n -= 10;
                }
                break;
            case 3:
                n = (n * n) % 100;
                break;
            default:
                break;
        }

        if (n > 100) {
            n -= 100;
        } else if (n < -100) {
            n += 200;
        } else {
            break;
        }

        iterations++;
    }

    printf("Final value of n: %d\n", n);
    return n;
}

char __attribute__((annotate("nookie:43"))) theNookie = 'a';

__attribute__((annotate("flatten")))
__attribute__((annotate("bogus-switch")))
int main(int argc, char **argv) {
    int n = atoi(argv[1]);
    return foo(n);
}
