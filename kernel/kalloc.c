// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PG2REFIDX(_pa) (((_pa) - KERNBASE) / PGSIZE)  // 获取下标
#define MX_PGIDX PG2REFIDX(PHYSTOP)   // 获取最大下标

int pg_refcnt[MX_PGIDX];  // 标记数组

#define PG_REFCNT(_pa) pg_refcnt[PG2REFIDX((uint64)(_pa))]  // 宏定义

struct spinlock refcnt_lock;

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt_lock, "ref cnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    PG_REFCNT(p) = 0;   // 将该pte置空
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");


  // 首先减少记数，如果小于等于0则进行回收
  acquire(&refcnt_lock);
  if (--PG_REFCNT(pa) <= 0) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&refcnt_lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;   // 指针r获取freelist一块空闲的内存后，freelist指针指向后一个空闲内存
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    PG_REFCNT(r) = 1;   // 分配时总共有一个进程使用这个页帧，所以置为 1 。
  }
    
  return (void*)r;
}


// lab 2-2
// 获取空闲的内存数量
uint64
free_mem(void) {
  struct run *r = kmem.freelist;
  uint64 n = 0;
  while (r) {
    r = r->next;
    n++;
  }

  return n * PGSIZE;
}

// lab 5-1
void
refcnt_inc(void* pa)
{
  acquire(&refcnt_lock);
  PG_REFCNT(pa)++;
  release(&refcnt_lock);
}

void
refcnt_dec(void* pa)
{
  acquire(&refcnt_lock);
  PG_REFCNT(pa)--;
  release(&refcnt_lock);
}
