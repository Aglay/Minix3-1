/* This file contains code for initialization of protected mode, to initialize
 * code and data segment descriptors, and to initialize global descriptors
 * for local descriptors in the process table.
 */

#include "kernel.h"
#include "proc.h"
#include "protect.h"

#define INT_GATE_TYPE	(INT_286_GATE | DESC_386_BIT)
#define TSS_TYPE	(AVL_286_TSS  | DESC_386_BIT)

/* GDTR 寄存器内容(指向全局描述符表的开始)  */
struct desctableptr_s {
  char limit[sizeof(u16_t)];	/* 全局描述符表的段大小 */
  char base[sizeof(u32_t)];	/* 全局描述符表的段基地址 really u24_t + pad for 286 */
};

/* 中断描述符
 任务门 type  00101
 中断门       01110
 陷阱门       01111
 中断与异常的主要区别在于中断常用于处理 CPU 外部的异步事件,而异常
 则是处理 CPU 在执行过程中本身检测到的问题.
*/
struct gatedesc_s {
  u16_t offset_low;	        /* 低偏移值 */
  u16_t selector;	        /* 段选择符 */
  u8_t pad;			/* |000|XXXXX| ig & trpg, |XXXXXXXX| task g */
  u8_t p_dpl_type;		/* |P|DL|0|TYPE| */
  u16_t offset_high;            /* 高偏移值 */
};


/* 任务状态段 */
/*SS0:ESP0 用于存放任务在内核态运行时的堆栈指针。SS1:ESP1 和 SS2:ESP2 分别对应运行于
特权级 1 和 2 时使用的堆栈指针,这两个特权级在 Linux 中没有使用。而任务工作于用户态时堆栈指针
则保存在 SS:ESP 寄存器中
*/

struct tss_s {
  reg_t backlink;              /* 前一执行任务 TSS 的描述符 */
  reg_t sp0;                    /* stack pointer to use during interrupt */
  reg_t ss0;                    /*   "   segment  "  "    "        "     */
  reg_t sp1;
  reg_t ss1;
  reg_t sp2;
  reg_t ss2;
  reg_t cr3;                  /* 页目录基地址寄存器PDBR */
  reg_t ip;
  reg_t flags;
  reg_t ax;
  reg_t cx;
  reg_t dx;
  reg_t bx;
  reg_t sp;
  reg_t bp;
  reg_t si;
  reg_t di;
  reg_t es;
  reg_t cs;
  reg_t ss;
  reg_t ds;
  reg_t fs;
  reg_t gs;
  reg_t ldt;			/* -----下面这三个好像表里面没有----- */
  u16_t trap;
  u16_t iobase;
/* u8_t iomap[0]; */
};

PUBLIC struct segdesc_s gdt[GDT_SIZE];		/* used in klib.s and mpx.s */
PRIVATE struct gatedesc_s idt[IDT_SIZE];	/* zero-init so none present */
PUBLIC struct tss_s tss;			/* zero init */

FORWARD _PROTOTYPE( void int_gate, (unsigned vec_nr, vir_bytes offset,
		unsigned dpl_type) );
FORWARD _PROTOTYPE( void sdesc, (struct segdesc_s *segdp, phys_bytes base,
		vir_bytes size) );

/*===========================================================================*
 *				prot_init				     *
 *===========================================================================*/
PUBLIC void prot_init()
{
/* Set up tables for protected mode.
 * All GDT slots are allocated at compile time.
 */
  struct gate_table_s *gtp;
  struct desctableptr_s *dtp;
  unsigned ldt_index;
  register struct proc *rp;

  static struct gate_table_s {	/* 这个结构只是在这里使用了的 */
	_PROTOTYPE( void (*gate), (void) );
        unsigned char vec_nr;//中断向量
	unsigned char privilege;
  }
  gate_table[] = {
       /* ---中断向量在include/ibm/interrupt.h中定义----中断向量0->4---- */
	{ divide_error, DIVIDE_VECTOR, INTR_PRIVILEGE },
	{ single_step_exception, DEBUG_VECTOR, INTR_PRIVILEGE },
	{ nmi, NMI_VECTOR, INTR_PRIVILEGE },
	{ breakpoint_exception, BREAKPOINT_VECTOR, USER_PRIVILEGE },
	{ overflow, OVERFLOW_VECTOR, USER_PRIVILEGE },
	/* ----中断向量在kernel/protect.h中定义----中断向量5->16------ */
	{ bounds_check, BOUNDS_VECTOR, INTR_PRIVILEGE },
	{ inval_opcode, INVAL_OP_VECTOR, INTR_PRIVILEGE },
	{ copr_not_available, COPROC_NOT_VECTOR, INTR_PRIVILEGE },
	{ double_fault, DOUBLE_FAULT_VECTOR, INTR_PRIVILEGE },
	{ copr_seg_overrun, COPROC_SEG_VECTOR, INTR_PRIVILEGE },
	{ inval_tss, INVAL_TSS_VECTOR, INTR_PRIVILEGE },
	{ segment_not_present, SEG_NOT_VECTOR, INTR_PRIVILEGE },
	{ stack_exception, STACK_FAULT_VECTOR, INTR_PRIVILEGE },
	{ general_protection, PROTECTION_VECTOR, INTR_PRIVILEGE }, /* 13 */
	{ page_fault, PAGE_FAULT_VECTOR, INTR_PRIVILEGE }, /* 14 注意没有15*/
	{ copr_error, COPROC_ERR_VECTOR, INTR_PRIVILEGE }, /* 16 */
	/* ------VECTOR( )在ibm中定义--------- *//*-----上面的是异常,下面的是硬件中断-----*/
	{ hwint00, VECTOR( 0), INTR_PRIVILEGE }, /* 64 */
	{ hwint01, VECTOR( 1), INTR_PRIVILEGE },
	{ hwint02, VECTOR( 2), INTR_PRIVILEGE },
	{ hwint03, VECTOR( 3), INTR_PRIVILEGE },
	{ hwint04, VECTOR( 4), INTR_PRIVILEGE },
	{ hwint05, VECTOR( 5), INTR_PRIVILEGE },
	{ hwint06, VECTOR( 6), INTR_PRIVILEGE },
	{ hwint07, VECTOR( 7), INTR_PRIVILEGE }, /*71 ----64->71------- */
	{ hwint08, VECTOR( 8), INTR_PRIVILEGE }, /* 112 */
	{ hwint09, VECTOR( 9), INTR_PRIVILEGE },
	{ hwint10, VECTOR(10), INTR_PRIVILEGE },
	{ hwint11, VECTOR(11), INTR_PRIVILEGE },
	{ hwint12, VECTOR(12), INTR_PRIVILEGE },
	{ hwint13, VECTOR(13), INTR_PRIVILEGE },
	{ hwint14, VECTOR(14), INTR_PRIVILEGE },
	{ hwint15, VECTOR(15), INTR_PRIVILEGE },   /* 119---112->119----- */
	{ s_call, SYS386_VECTOR, USER_PRIVILEGE },	/* 386 system call :vector33 */
	{ level0_call, LEVEL0_VECTOR, TASK_PRIVILEGE }, /* :vector34for execution of a function at level 0 */
  };

  /* Build gdt and idt pointers in GDT where the BIOS expects them. */
  /* 我不知道上面的 where the BIOS expects them 指的是什么:可能就是BIOS要求GDT的顺序*/
  dtp= (struct desctableptr_s *) &gdt[GDT_INDEX];
  * (u16_t *) dtp->limit = (sizeof gdt) - 1;
  * (u32_t *) dtp->base = vir2phys(gdt); /* 在内核的数据段里面，加上基地址(我有点疑问，gdt是一个偏移地址吗?它究竟是个什么地址?) */

  dtp= (struct desctableptr_s *) &gdt[IDT_INDEX];
  * (u16_t *) dtp->limit = (sizeof idt) - 1;
  * (u32_t *) dtp->base = vir2phys(idt);

  /* Build segment descriptors for tasks and interrupt handlers. */
  /* 初始化任务和中断处理器的段寄存器描述符 */
  init_codeseg(&gdt[CS_INDEX],
  	 kinfo.code_base, kinfo.code_size, INTR_PRIVILEGE);
  init_dataseg(&gdt[DS_INDEX],
  	 kinfo.data_base, kinfo.data_size, INTR_PRIVILEGE);
  init_dataseg(&gdt[ES_INDEX], 0L, 0, TASK_PRIVILEGE);  /* size参数为0代表段限长为4G */

  /* Build scratch descriptors for functions in klib88. */
  init_dataseg(&gdt[DS_286_INDEX], 0L, 0, TASK_PRIVILEGE);
  init_dataseg(&gdt[ES_286_INDEX], 0L, 0, TASK_PRIVILEGE);

  /* 局部描述符表是每个进程要用的，每个进程都有一个自己的局部描述符 */
  /* Build local descriptors in GDT for LDT's in process table.
   * The LDT's are allocated at compile time in the process table, and
   * initialized whenever a process' map is initialized or changed.
   */
  /*从ldt_index = FIRST_LDT_INDEX可以知道LDT是从GDT的最后一项开始的 */
  for (rp = BEG_PROC_ADDR, ldt_index = FIRST_LDT_INDEX;
       rp < END_PROC_ADDR; ++rp, ldt_index++) {
	init_dataseg(&gdt[ldt_index], vir2phys(rp->p_ldt),
				     sizeof(rp->p_ldt), INTR_PRIVILEGE);
	gdt[ldt_index].access = PRESENT | LDT;
	rp->p_ldt_sel = ldt_index * DESC_SIZE; /* 还初始化了在全局描述表中的局部描述符的选择符符 */
  }

  /* Build main TSS.
   * This is used only to record the stack pointer to be used after an
   * interrupt.
   * The pointer is set up so that an interrupt automatically saves the
   * current process's registers ip:cs:f:sp:ss in the correct slots in the
   * process table.
   *
   * 图中 SS0:ESP0 用于存放任务在内核态运行时的堆栈指针。SS1:ESP1 和 SS2:ESP2 分别对应运行于
   * 特权级 1 和 2 时使用的堆栈指针,这两个特权级在 Linux 中没有使用。而任务工作于用户态时堆栈指针
   * 则保存在 SS:ESP 寄存器中。
  */

  tss.ss0 = DS_SELECTOR; /* 怎么只是设置了一个(SS0:ESP0 用于存放任务在内核态运行时的堆栈指针) */
  init_dataseg(&gdt[TSS_INDEX], vir2phys(&tss), sizeof(tss), INTR_PRIVILEGE);
  gdt[TSS_INDEX].access = PRESENT | (INTR_PRIVILEGE << DPL_SHIFT) | TSS_TYPE;

  /* Build descriptors for interrupt gates in IDT. */
  for (gtp = &gate_table[0];
       gtp < &gate_table[sizeof gate_table / sizeof gate_table[0]]; ++gtp) {
	int_gate(gtp->vec_nr, (vir_bytes) gtp->gate,
		 PRESENT | INT_GATE_TYPE | (gtp->privilege << DPL_SHIFT));
  }

  /* Complete building of main TSS. */
  tss.iobase = sizeof tss;	/* empty i/o permissions map */
}

/*===========================================================================*
 *				init_codeseg				     *
 *===========================================================================*/
PUBLIC void init_codeseg(segdp, base, size, privilege)
register struct segdesc_s *segdp;
phys_bytes base;
vir_bytes size;
int privilege;
{
/* Build descriptor for a code segment. */
  sdesc(segdp, base, size);
/*用于程序代码段和数据段的描述符access中SEGMENT这个位要设置为1 */
  segdp->access = (privilege << DPL_SHIFT)
	        | (PRESENT | SEGMENT | EXECUTABLE | READABLE);
		/* CONFORMING = 0, ACCESSED = 0 */
}

/*===========================================================================*
 *				init_dataseg				     *
 *===========================================================================*/
PUBLIC void init_dataseg(segdp, base, size, privilege)
register struct segdesc_s *segdp;
phys_bytes base;
vir_bytes size;
int privilege;
{
/* Build descriptor for a data segment. */
  sdesc(segdp, base, size);
  segdp->access = (privilege << DPL_SHIFT) | (PRESENT | SEGMENT | WRITEABLE);
		/* EXECUTABLE = 0, EXPAND_DOWN = 0, ACCESSED = 0 */
}

/*===========================================================================*
 *				sdesc					     *
 *===========================================================================*/
PRIVATE void sdesc(segdp, base, size)
register struct segdesc_s *segdp;
phys_bytes base;
vir_bytes size;
{
/* Fill in the size fields (base, limit and granularity) of a descriptor. */
  segdp->base_low = base;
  segdp->base_middle = base >> BASE_MIDDLE_SHIFT;
  segdp->base_high = base >> BASE_HIGH_SHIFT;

  --size;			/* convert to a limit, 0 size means 4G */
  if (size > BYTE_GRAN_MAX) {	/* 大于字节单元颗粒度的最大值1M，所以是4kb的颗粒度 */
	segdp->limit_low = size >> PAGE_GRAN_SHIFT;
	segdp->granularity = GRANULAR | (size >>
				     (PAGE_GRAN_SHIFT + GRANULARITY_SHIFT));
  } else {
	segdp->limit_low = size;
	segdp->granularity = size >> GRANULARITY_SHIFT;
  }

  /* 设置描述符表中的X位，但我还不知道这个位的作用是什么 */
  segdp->granularity |= DEFAULT;	/* means BIG for data seg */
}

/*===========================================================================*
 *				seg2phys				     *
 *===========================================================================*/
PUBLIC phys_bytes seg2phys(seg)
U16_t seg;
{
/* Return the base address of a segment, with seg being either a 8086 segment
 * register, or a 286/386 segment selector.
 */
  phys_bytes base;
  struct segdesc_s *segdp;

  if (! machine.protected) {
	base = hclick_to_physb(seg);
  } else {
	segdp = &gdt[seg >> 3];
	base =    ((u32_t) segdp->base_low << 0)
		| ((u32_t) segdp->base_middle << 16)
		| ((u32_t) segdp->base_high << 24);
  }
  return base;
}

/*===========================================================================*
 *				phys2seg(已经不再需要了)				     * 
 *===========================================================================*/
PUBLIC void phys2seg(seg, off, phys)
u16_t *seg;
vir_bytes *off;
phys_bytes phys;
{
/* Return a segment selector and offset that can be used to reach a physical
 * address, for use by a driver doing memory I/O in the A0000 - DFFFF range.
 */
  *seg = FLAT_DS_SELECTOR;
  *off = phys;
}

/*===========================================================================*
 *				int_gate				     *
 *===========================================================================*/
PRIVATE fvoid int_gate(vec_nr, offset, dpl_type)
unsigned vec_nr;
vir_bytes offset;
unsigned dpl_type;
{
/* Build descriptor for an interrupt gate. */
  register struct gatedesc_s *idp;

  idp = &idt[vec_nr];
  idp->offset_low = offset;
  idp->selector = CS_SELECTOR;
  idp->p_dpl_type = dpl_type;
  idp->offset_high = offset >> OFFSET_HIGH_SHIFT;
}

/*===========================================================================*
 *				enable_iop				     * 
 *===========================================================================*/
PUBLIC void enable_iop(pp)
struct proc *pp;
{
/* Allow a user process to use I/O instructions.  Change the I/O Permission
 * Level bits in the psw. These specify least-privileged Current Permission
 * Level allowed to execute I/O instructions. Users and servers have CPL 3. 
 * You can't have less privilege than that. Kernel has CPL 0, tasks CPL 1.
 */
  pp->p_reg.psw |= 0x3000;
}

/*===========================================================================*
 *				alloc_segments				     *
 *===========================================================================*/
PUBLIC void alloc_segments(rp)
register struct proc *rp;
{
/* This is called at system initialization from main() and by do_newmap(). 
 * The code has a separate function because of all hardware-dependencies.
 * Note that IDLE is part of the kernel and gets TASK_PRIVILEGE here.
 */
  phys_bytes code_bytes;
  phys_bytes data_bytes;
  int privilege;

  if (machine.protected) {
      data_bytes = (phys_bytes) (rp->p_memmap[S].mem_vir + 
          rp->p_memmap[S].mem_len) << CLICK_SHIFT;
      if (rp->p_memmap[T].mem_len == 0)
          code_bytes = data_bytes;	/* common I&D, poor protect */
      else
          code_bytes = (phys_bytes) rp->p_memmap[T].mem_len << CLICK_SHIFT;
      privilege = (iskernelp(rp)) ? TASK_PRIVILEGE : USER_PRIVILEGE;
      init_codeseg(&rp->p_ldt[CS_LDT_INDEX],
          (phys_bytes) rp->p_memmap[T].mem_phys << CLICK_SHIFT,
          code_bytes, privilege);
      init_dataseg(&rp->p_ldt[DS_LDT_INDEX],
          (phys_bytes) rp->p_memmap[D].mem_phys << CLICK_SHIFT,
          data_bytes, privilege);
      rp->p_reg.cs = (CS_LDT_INDEX * DESC_SIZE) | TI | privilege;
      rp->p_reg.gs =
      rp->p_reg.fs =
      rp->p_reg.ss =
      rp->p_reg.es =
      rp->p_reg.ds = (DS_LDT_INDEX*DESC_SIZE) | TI | privilege;
  } else {
      rp->p_reg.cs = click_to_hclick(rp->p_memmap[T].mem_phys);
      rp->p_reg.ss =
      rp->p_reg.es =
      rp->p_reg.ds = click_to_hclick(rp->p_memmap[D].mem_phys);
  }
}

