#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    // 检测是否有其他程序杀掉了当前程序
    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    /*
      在RISC-V中，存储在SEPC寄存器中的程序计数器，是用户程序中触发trap的指令的地址。
      但是当我们恢复用户程序时，我们希望在下一条指令恢复，也就是ecall之后的一条指令。
      所以对于系统调用，我们对于保存的用户程序计数器加4，
      这样我们会在ecall的下一条指令恢复，而不是重新执行ecall指令。
    */
    p->trapframe->epc += 4;   // epc: saved user program counter

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    /*
      XV6会在处理系统调用的时候使能中断，这样中断可以更快的服务，
      有些系统调用需要许多时间处理。中断总是会被RISC-V的trap硬件关闭，
      所以在这个时间点，我们需要显式的打开中断。
    */
    intr_on();

    // 准备工作完成，执行系统调用
    syscall();

  }
  else if (r_scause() == 15) {
    // Page Fault
    uint64 va = r_stval();  // Supervisor Trap Value 触发中断的地址
    printf("page fault at va: %p\n", va);

    uint64 ka = (uint64)kalloc();  // Allocate one 4096-byte page of physical memory.
    if (ka == 0) {
      // 申请内存失败
      p->killed = 1;
    } else {
      memset((void*)ka, 0, PGSIZE); // 将新申请的内存初始化为0
      va = PGROUNDDOWN(va);         // 将进程地址空间指针指向低位置
      // 将用户空间的地址映射到新申请的物理地址上去
      if (mappages(p->pagetable, va, PGSIZE, ka, PTE_W | PTE_U | PTE_R) != 0) {
        // 注意！设置的权限里面没有PTE_V，所以在回收内存的时候，uvmunmap(vm.c)会没有PTE_V标记的pte抛出异常，需要修改uvmunmap函数的逻辑才能正确执行程序
          // 申请失败
          kfree((void*)ka);
          p->killed = 1;
      }
    }
  }
  else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  // 再次检测当前进程是否被杀掉
  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2) {        // 编号2，说明该中断是时钟中断
    if (p->alarm_tks > 0) {
      p->alarm_tk_elapsed++;    // 距离上次handler执行的时间
      if (p->alarm_tk_elapsed > p->alarm_tks && !p->alarm_state) {
        // 当前已执行的时间超过了规定的时间, 并且handler没有执行
        p->alarm_tk_elapsed = 0;
        *p->alarmframe = *p->trapframe;   // 备份当前环境
        p->trapframe->epc = p->alarm_handler;   // 直接改 epc，这样回用户态的时候就会执行地址为 epc 的指令
        p->alarm_state = 1;   // 标记当前handler正在执行
      }
    }

    // Give up the CPU for one scheduling round.
    yield();
  }

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);  // 因为内核空间和用户空间关于trampoline page映射相同，所以此处无需对地址做特殊处理
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

