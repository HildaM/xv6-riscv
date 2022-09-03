#include "kernel/types.h"
#include "user/user.h"

#define READ 0
#define WRITE 1
#define stdin 0
#define stdout 1
#define stderr 2


/*
    正解：
        1. 创建两个pipe
        2. p1: 父进程写，子进程读
        3. p2: 父进程读，子进程写
        4. 最后关闭所有pipe
*/
int main(int argc, char* argv[]) {
    int pid, p2c[2], c2p[2];
    char buf[] = "hello world";

    pipe(p2c);
    pipe(c2p);

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "ERROR: fork error\n");
        exit(1);
    }
    else if (pid > 0) {
        // parent
        // close neccesary pipe
        close(p2c[READ]);
        close(c2p[WRITE]);

        write(p2c[WRITE], &buf, 1);
        close(p2c[WRITE]);

        // wait child exit
        wait(0);

        read(c2p[READ], &buf, 1);
        close(c2p[WRITE]);

        printf("PID %d: receive pong: %s\n", getpid(), buf);
        exit(0);
    }
    else if (pid == 0) {
        // child
        close(p2c[WRITE]);
        close(c2p[READ]);

        read(p2c[READ], &buf, 1);
        close(p2c[READ]);
        printf("PID %d: receive ping\n", getpid());

        write(c2p[WRITE], &buf, 1);
        close(c2p[WRITE]);

        exit(0);
    }

    exit(0);
}



/*
    问题：
        1. 父子进程不能先后执行
        2. 父进程无法接收子进程的数据，说明pipe操作有问题！
*/
void myOwnTry() {
        // 1. create a pipe
    int p[2];   
    pipe(p);    // read fd put into p[0], write fd put into p[1]

    if (fork() == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);    // close read pipe
        int pid = getpid();
        printf("<PID: %d>: receive ping\n", pid);
        write(p[1], "hello world", 11);
        close(p[1]);    // close write pipe
        
        wait(0);
        exit(0);
    } else {
        close(p[0]);

        // get message from child
        char* message;
        read(p[0], &message, 11);
        close(p[0]);
        close(p[1]);

        int pid = getpid();
        printf("<PID %d>: receive pong from child: %s\n", pid, message);

        wait(0);
        exit(0);
    }

    exit(0);
}