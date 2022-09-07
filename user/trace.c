#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define stdin   0
#define stdout  1
#define stderr  2

int
main(int argc, char* argv[]) {
    char *nargv[MAXARG];

    // 检测参数个数 + 检测mask格式
    if (argc < 3 || (argv[1][0] < '0' || argv[1][0] > '9')) {
        fprintf(stderr, "Usage: %s mask command\n", argv[0]);
        exit(1);
    }

    // trace 系统调用
    if (trace(atoi(argv[1])) < 0) {
        fprintf(stderr, "%s: trace failed\n", argv[0]);
        exit(1);
    }

    // 设置要跟踪程序的参数
    for (int i = 2; i < argc && i < MAXARG; i++) {      // i < argc && i < MAXARG !!!!!
        // nargv只保留参数，不保持argv前面两个参数
        // argv[0] = trace  argv[1] = mask
        nargv[i - 2] = argv[i];
    }

    // 执行系统调用
    exec(nargv[0], nargv);
    exit(0);
}