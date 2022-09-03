#include "kernel/types.h"
#include "user/user.h"

int parse_int(char* s) {
    const char* p = s;
    for (; *p; p++) {
        if (*p < '0' || *p > '9') return -1;
    }

    return atoi(s);     // atoi 字符串转换为int
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(2, "ERROR: Sleep time required\n");
        exit(1);
    }

    int time = parse_int(argv[1]);   
    if (time < 0) {
        fprintf(2, "ERROR: time grgument: %s\n", argv[1]);
        exit(1);
    }

    sleep(time);   
    exit(0);
}