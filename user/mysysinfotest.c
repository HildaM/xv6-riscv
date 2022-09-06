#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "kernel/sysinfo.h"
#include "kernel/riscv.h"

#define stdin 0
#define stdout 1
#define stderr 2

int
main(int argc, char* argv[]) {
    struct sysinfo info;
    
    // 调用系统函数
    if (sysinfo(&info) < 0) {
        fprintf(stderr, "sysinfo error\n");
        exit(1);
    }

    printf("Free memory: %d MB, the num of unused processes: %d\n", info.freemem, info.nproc);
    exit(0);
}