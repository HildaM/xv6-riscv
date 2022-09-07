#include "kernel/types.h"
#include "user/user.h"

#define PRIME_NUM 35
#define READ 0
#define WRITE 1

void child(int* pl);

int main(int argc, char* argv[]) {
    int p[2];
    pipe(p);

    if (fork() == 0) {
        // child
        child(p);   // 递归
    } else {
        // parend
        close(p[READ]);
        for (int i = 2; i <= PRIME_NUM; i++) {
            write(p[WRITE], &i, sizeof(i));
        }
        close(p[WRITE]);

        // wait for every child exit
        wait((int*) 0);
    }

    exit(0);
}

void child(int* pl) {
    // 1. get left pipe result
    int n;
    close(pl[WRITE]);
    int res = read(pl[READ], &n, sizeof(int));
    if (res == 0) {     // 递归退出
        exit(0);
    }
        
    int pr[2];  // right pipe
    pipe(pr);

    // 2. continue to prime
    if (fork() == 0) {
        // child of child
        child(pr);
    } else {
        close(pr[READ]);
        printf("prime: %d\n", n);
        int prime = n;

        while (read(pl[READ], &n, sizeof(int)) != 0) {
            if (n % prime != 0) {
                // 可以被整除的，drop it
                write(pr[WRITE], &n, sizeof(int));
            }
        }

        close(pr[WRITE]);
        // wait for its child to exit
        wait((int*) 0);

        exit(0);
    }
}
