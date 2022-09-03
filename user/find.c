#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define stdin 0
#define stdout 1
#define stderr 2
#define READ 0
#define WRITE 1

void find(char* path, char* target_file);

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "ERROR: The minimum argument is 2\n");
        exit(1);
    }

    char* target_path = argv[1];
    char* target_file = argv[2];
    find(target_path, target_file);

    exit(0);
}

// 获取路径下的文件名
// path = "/a/b/c"  return c
char* fmtName(char* path) {
    char* p;
    // 移动字符串指针到倒数第一个 ‘/’ 上
    for (p = path + strlen(path); p >= path && *p != '/'; --p);
    return p + 1;
}

void find(char* path, char* target_file) {
    // 参考ls.c
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 1. 参数检测
    // 打开文件，获取path文件描述符
    if ((fd = open(path, 0)) < 0) {
        fprintf(stderr, "ls: cannot open %s\n", path);
        return;
    }
    
    // 获取path文件状态stat，检测stat是否正常
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }


    // 2. 根据不同文件类型处理
    switch (st.type)
    {
    case T_FILE:
        if (strcmp(fmtName(path), target_file) == 0) {
            // 找到文件
            printf("%s\n", path);
        }
        break;
    
    case T_DIR:
        // if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){   // len(buf) == 512，文件夹长度不能大于512
        //     printf("ls: path too long\n");
        //     break;
        // }

        strcpy(buf, path);
        p = buf + strlen(buf);      // 拓展长度
        *p++ = '/';     // 添加下划线

        // 读取fd下的所有文件，将信息存储到de中
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) 
                continue;
            printf("Debug: de.name: %s\n", de.name);
            memmove(p, de.name, DIRSIZ);     // 更新p的路径 ---> /a/b ---> /a/b/c
            p[DIRSIZ] = 0;
            find(buf, target_file);
        }

        break;
    }

    close(fd);
    // exit(0) 不可以在此使用exit，否则会提前退出程序！！！
}
