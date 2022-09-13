#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"


uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


// sys_trace 跟踪系统调用
uint64
sys_trace(void) {
  int mask;
  argint(0, &mask);           // 获取系统调用的编码
  if (mask < 0) return -1;    // 判断该编码是否正确

  myproc()->mask = mask;   // 设置当前线程的mask
  return 0;
}


// sys_info 返回系统进程部分信息
uint64
sys_info(void) {
  struct sysinfo info;
  uint64 addr;
  struct proc* p = myproc();

  argaddr(0, &addr);    // 获取a0寄存器的指针
  if (addr < 0) return -1;

  // 1. 获取空闲内存
  info.freemem = free_mem();

  // 2. 查看空闲的线程数
  info.nproc = nproc_num();

  // info数据的在用户空间与内核空间的虚拟内存地址是一样的
  // 这样我们就能够通过这个一样的虚拟地址，从内核空间中，直接写入到用户空间pagetable中相同的地方
  // copyout就是在干这件事情！从info地址中，读取sizeof(info)大小的数据，写入到当前线程的pagetable对应的地址处（a0寄存器：addr地址）
  if (copyout(p->pagetable, addr, (char*)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}


// sys_pgaccess 
uint64
sys_pgaccess(void)
{
  uint64 userpage_ptr;   // 待检测页表起始指针
  int page_num;          // 待检测页表数量
  uint64 ans;            // 稍后写入用户的内存地址

  // 获取用户态的参数
  argaddr(0, &userpage_ptr);
  if (userpage_ptr < 0) return -1;

  argint(1, &page_num);
  if (page_num < 0) return -1;

  argaddr(2, &ans);
  if (ans < 0) return -1;

  return pgaccess((void *)userpage_ptr, page_num, (void *)ans);
}