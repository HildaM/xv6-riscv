#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define stdin 0
#define stdout 1
#define stderr 2
#define READ 0
#define WRITE 1
#define MAX_ARG_LEN 1024

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: xargs executable [options]....\n");
        exit(1);
    }

    if (argc > MAXARG) {
        fprintf(stderr, "ERROR: Arguments %d out of bound %d\n", argc, MAXARG);
        exit(1);
    }

    int pid, n, buf_index = 0;
    char buf, arg[MAX_ARG_LEN], *args[MAXARG];

    // 1. 将管道符后面的参数放到args中
    for (int i = 1; i < argc; i++) args[i - 1] = argv[i];   // argv[0] = 'xargs'

    /*
        注意在执行xargs这个命令行的时候，
        最后肯定要按一个回车，这时标准输入最后会有一个回车，所以在EOF前是会有一个回车的！！！
    */
    args[argc - 1] = arg;   // 空行，相当于回车
    args[argc] = 0;     // EOF 文件末尾

    // Debug for argv
    // for (int i = 0; i < argc; i++) {
    //     printf("Debug: argv[%d] = %s\n", i, argv[i]);
    // }
    // Debug for args
    // for (int i = 0; i < MAXARG && args[i] != 0; i++) {
    //     printf("Debug: args[%d] = %s\n", i, args[i]);
    // }

    // 2. 监控命令行，直到遇到换行符
    while ((n = read(stdin, &buf, 1)) > 0) {    // 逐个字符读取
        if (buf == '\n' || buf == ' ') {
            arg[buf_index] = 0;

            if ((pid = fork()) < 0) {
                fprintf(stderr, "fork error\n");
                exit(1);
            } else if (pid == 0) {
                // child
                exec(args[0], args);    // 执行保持在args中的命令
            } else {
                // parent
                wait(0);
                buf_index = 0;
            }
        } else {
            arg[buf_index++] = buf;
        }
    }

    if (n < 0) {
        fprintf(stderr, "read error\n");
        exit(1);
    }

    exit(0);
}