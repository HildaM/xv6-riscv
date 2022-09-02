#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc == 1) {
        fprintf(2, "ERROR: Sleep time required\n");
        exit(1);
    }
    sleep(atoi(argv[1]));   // atoi 字符串转换为int
    exit(0);
}