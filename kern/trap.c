#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/time.h>

extern uintptr_t gdtdesc_64;
struct Taskstate ts;
extern struct Segdesc gdt[];
extern long gdt_pd;

/* For debugging, so print_trapframe can distinguish between printing
 * a saved trapframe and printing the current trapframe and print some
 * additional information in the latter case.
 */
static struct Trapframe *last_tf;

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted function addresses can't be represented in relocation records.)
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {0, 0};

extern void trph0();
extern void trph1();
extern void trph2();
extern void trph3();
extern void trph4();
extern void trph5();
extern void trph6();
extern void trph7();
extern void trph8();
//extern void trph9();
extern void trph10();
extern void trph11();
extern void trph12();
extern void trph13();
extern void trph14();
//extern void trph15();
extern void trph16();
extern void trph17();
extern void trph18();
extern void trph19();
extern void trph48();
extern void trph_default();

extern void irq0();
extern void irq1();
extern void irq2();

static const char *trapname(int trapno)
{
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if (trapno < sizeof(excnames)/sizeof(excnames[0]))
		return excnames[trapno];
	if (trapno == T_SYSCALL)
		return "System call";
	if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}


void
trap_init(void)
{
	extern struct Segdesc gdt[];

	idt_pd.pd_lim = sizeof(idt) - 1;
	idt_pd.pd_base = (uint64_t)idt;

	//set every trap to the default
	int i = 20;
	for (; i < 256; i++){
		SETGATE(idt[i],0,GD_KT,trph_default,0);
	}

	//set our specific trap handlers.
	SETGATE(idt[0],0,GD_KT,trph0,0);
	SETGATE(idt[1],0,GD_KT,trph1,0);
	SETGATE(idt[2],0,GD_KT,trph2,0);
	SETGATE(idt[3],0,GD_KT,trph3,3);
	SETGATE(idt[4],0,GD_KT,trph4,0);
	SETGATE(idt[5],0,GD_KT,trph5,0);
	SETGATE(idt[6],0,GD_KT,trph6,0);
	SETGATE(idt[7],0,GD_KT,trph7,0);
	SETGATE(idt[8],0,GD_KT,trph8,0);
	//SETGATE(idt[9],1,GD_KT,trph9,0);
	SETGATE(idt[10],0,GD_KT,trph10,0);
	SETGATE(idt[11],0,GD_KT,trph11,0);
	SETGATE(idt[12],0,GD_KT,trph12,0);
	SETGATE(idt[13],0,GD_KT,trph13,0);
	SETGATE(idt[14],0,GD_KT,trph14,0);
	//SETGATE(idt[15],1,GD_KT,trph15,0);
	SETGATE(idt[16],0,GD_KT,trph16,0);
	SETGATE(idt[17],0,GD_KT,trph17,0);
	SETGATE(idt[18],0,GD_KT,trph18,0);
	SETGATE(idt[19],0,GD_KT,trph19,0);
		
	SETGATE(idt[48],0,GD_KT,trph48,3);	
	// Per-CPU setup

	SETGATE(idt[IRQ_OFFSET + 0],0,GD_KT,irq0,0);
	SETGATE(idt[IRQ_OFFSET + IRQ_KBD],0,GD_KT,irq1,0);
	SETGATE(idt[IRQ_OFFSET + IRQ_SERIAL],0,GD_KT,irq2,0);
	trap_init_percpu();
}

// Initialize and load the per-CPU TSS and IDT
void
trap_init_percpu(void)
{
	// The example code here sets up the Task State Segment (TSS) and
	// the TSS descriptor for CPU 0. But it is incorrect if we are
	// running on other CPUs because each CPU has its own kernel stack.
	// Fix the code so that it works for all CPUs.
	//
	// Hints:
	//   - The macro "thiscpu" always refers to the current CPU's
	//     struct CpuInfo;
	//   - The ID of the current CPU is given by cpunum() or
	//     thiscpu->cpu_id;
	//   - Use "thiscpu->cpu_ts" as the TSS for the current CPU,
	//     rather than the global "ts" variable;
	//   - Use gdt[(GD_TSS0 >> 3) + 2*i] for CPU i's TSS descriptor;
	//   - You mapped the per-CPU kernel stacks in mem_init_mp()
	//
	// ltr() sets a "busy" flag in the TSS selector, so if you
	// accidentally load the same TSS on more than one CPU, you'll
	// get a triple fault.  If you set up an individual CPU's TSS
	// incorrectly, you may not get a fault until you try to return from
	// user space on that CPU.
	//
	// LAB 4: Your code here:
	//TODO

	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	//ts.ts_esp0 = KSTACKTOP;
	unsigned int cpunumber = (unsigned int)cpunum();

	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpunumber*(KSTKSIZE + KSTKGAP);
	// Initialize the TSS slot of the gdt.
	SETTSS((struct SystemSegdesc64 *)&gdt[((GD_TSS0 >> 3) + (2*(cpunumber)))],STS_T64A, (uint64_t) (&thiscpu->cpu_ts),sizeof(struct Taskstate), 0);



	// Load the TSS selector (like other segment selectors, the
	// bottom three bits are special; we leave them 0)
	ltr(((GD_TSS0) + (2*cpunumber<<3)) & ~0x7);

	// Load the IDT
	lidt(&idt_pd);
}

void
print_trapframe(struct Trapframe *tf)
{
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  rip  0x%08x\n", tf->tf_rip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  rsp  0x%08x\n", tf->tf_rsp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void
print_regs(struct PushRegs *regs)
{
	cprintf("  r15  0x%08x\n", regs->reg_r15);
	cprintf("  r14  0x%08x\n", regs->reg_r14);
	cprintf("  r13  0x%08x\n", regs->reg_r13);
	cprintf("  r12  0x%08x\n", regs->reg_r12);
	cprintf("  r11  0x%08x\n", regs->reg_r11);
	cprintf("  r10  0x%08x\n", regs->reg_r10);
	cprintf("  r9  0x%08x\n", regs->reg_r9);
	cprintf("  r8  0x%08x\n", regs->reg_r8);
	cprintf("  rdi  0x%08x\n", regs->reg_rdi);
	cprintf("  rsi  0x%08x\n", regs->reg_rsi);
	cprintf("  rbp  0x%08x\n", regs->reg_rbp);
	cprintf("  rbx  0x%08x\n", regs->reg_rbx);
	cprintf("  rdx  0x%08x\n", regs->reg_rdx);
	cprintf("  rcx  0x%08x\n", regs->reg_rcx);
	cprintf("  rax  0x%08x\n", regs->reg_rax);
}

static void
trap_dispatch(struct Trapframe *tf)
{
	// Handle processor exceptions.
	// LAB 3: Your code here.

	// Handle spurious interrupts
	// The hardware sometimes raises these because of noise on the
	// IRQ line or other reasons. We don't care.
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS) {
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}
	
	if (tf->tf_trapno == T_PGFLT){
		page_fault_handler(tf);
		return;
	}

	if (tf->tf_trapno == T_ILLOP){
		cprintf("instruction: %x\n", tf->tf_rip);
	}
	
	if (tf->tf_trapno == T_SYSCALL){
		int res = 
		syscall(tf->tf_regs.reg_rax, 
		tf->tf_regs.reg_rdx, 
		tf->tf_regs.reg_rcx, 
		tf->tf_regs.reg_rbx,
		tf->tf_regs.reg_rdi,
		tf->tf_regs.reg_rsi
		);
		//pass the return value back in %rax.
		tf->tf_regs.reg_rax = res;
		//now we should return execution back to the 
		//program that triggered the interrupt
		//something with tss and iret.
		return;		
	}
	
	if (tf->tf_trapno == T_BRKPT){
		//while(1){monitor(tf);}
		return;
	}

	if (tf->tf_trapno == 13){
		cprintf("bizarre general protection fault\n");
	}

	if (tf->tf_trapno == IRQ_OFFSET + 0){
		lapic_eoi();
		
		sched_yield();
		return;
	}
	
	if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD){
		kbd_intr();
		return;
	}

	if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL){
		serial_intr();
		return;
	}

	// Add time tick increment to clock interrupts.
	// Be careful! In multiprocessors, clock interrupts are
	// triggered on every CPU.
	// LAB 6: Your code here.


	// Handle keyboard and serial interrupts.
	// LAB 5: Your code here.

	// Unexpected trap: The user process or the kernel has a bug.
	print_trapframe(tf);
	if (tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else {
		env_destroy(curenv);
		return;
	}
}

void
trap(struct Trapframe *tf)
{
	// The environment may have set DF and some versions
	// of GCC rely on DF being clear
	asm volatile("cld" ::: "cc");

	// Halt the CPU if some other CPU has called panic()
	extern char *panicstr;
	if (panicstr)
		asm volatile("hlt");

	// Re-acqurie the big kernel lock if we were halted in
	// sched_yield()
	if (xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();

	// Check that interrupts are disabled.  If this assertion
	// fails, DO NOT be tempted to fix it by inserting a "cli" in
	// the interrupt path.
	assert(!(read_eflags() & FL_IF));

	if ((tf->tf_cs & 3) == 3) {
		// Trapped from user mode.
		// Acquire the big kernel lock before doing any
		// serious kernel work.
		// LAB 4: Your code here.
		lock_kernel();
		assert(curenv);

		// Garbage collect if current enviroment is a zombie
		if (curenv->env_status == ENV_DYING) {
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// Copy trap frame (which is currently on the stack)
		// into 'curenv->env_tf', so that running the environment
		// will restart at the trap point.
		curenv->env_tf = *tf;
		// The trapframe on the stack should be ignored from here on.
		tf = &curenv->env_tf;
	}

	// Record that tf is the last real trapframe so
	// print_trapframe can print some additional information.
	last_tf = tf;
	
	// Dispatch based on what type of trap occurred
	trap_dispatch(tf);

	// If we made it to this point, then no other environment was
	// scheduled, so we should return to the current environment
	// if doing so makes sense.
	
	// DEBUG
	// this never executes on an AP (non-boot CPU)
	//cprintf("ZZZZZZZz cpunum(): %d, curenv: %x\n",cpunum(), curenv);
	if (curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}


void
page_fault_handler(struct Trapframe *tf)
{
	uint64_t fault_va;

	// Read processor's CR2 register to find the faulting address
	fault_va = rcr2();

	// Handle kernel-mode page faults.
	// LAB 3: Your code here.

	// We've already handled kernel-mode exceptions, so if we get here,
	// the page fault happened in user mode.
	
	assert(tf->tf_cs);

	// Call the environment's page fault upcall, if one exists.  Set up a
	// page fault stack frame on the user exception stack (below
	// UXSTACKTOP), then branch to curenv->env_pgfault_upcall.
	//
	// The page fault upcall might cause another page fault, in which case
	// we branch to the page fault upcall recursively, pushing another
	// page fault stack frame on top of the user exception stack.
	//
	// The trap handler needs one word of scratch space at the top of the
	// trap-time stack in order to return.  In the non-recursive case, we
	// don't have to worry about this because the top of the regular user
	// stack is free.  In the recursive case, this means we have to leave
	// an extra word between the current top of the exception stack and
	// the new stack frame because the exception stack _is_ the trap-time
	// stack.
	//
	//
	// If there's no page fault upcall, the environment didn't allocate a
	// page for its exception stack or can't write to it, or the exception
	// stack overflows, then destroy the environment that caused the fault.
	// Note that the grade script assumes you will first check for the page
	// fault upcall and print the "user fault va" message below if there is
	// none.  The remaining three checks can be combined into a single test.
	//
	// Hints:
	//   user_mem_assert() and env_run() are useful here.
	//   To change what the user environment runs, modify 'curenv->env_tf'
	//   (the 'tf' variable points at 'curenv->env_tf').

	// LAB 4: Your code here.
	typedef void func(void);
	func* pgfault_upcall = NULL;
	if (curenv && (curenv->env_pgfault_upcall)){
		pgfault_upcall = (func*)curenv->env_pgfault_upcall;
	}
	if (pgfault_upcall){
		struct UTrapframe* utf;
		
		// To test whether tf->tf_rsp is already on the user exception 
		// stack, check whether it is in the range 
		// [UXSTACKTOP-PGSIZE, UXSTACKTOP).
			
		if(tf->tf_rsp >= UXSTACKTOP - PGSIZE && tf->tf_rsp <= UXSTACKTOP){
			utf = (struct UTrapframe*)(tf->tf_rsp - 8 - sizeof(struct UTrapframe));
		}  
		else {
			utf = (struct UTrapframe*)(UXSTACKTOP-sizeof(struct UTrapframe));
		}
		user_mem_assert(curenv,(void*)utf,1,PTE_U);
		
		utf->utf_fault_va = fault_va;
		utf->utf_err = tf->tf_err;
		utf->utf_regs = tf->tf_regs;
		utf->utf_rip = tf->tf_rip;
		utf->utf_eflags = tf->tf_eflags;
		utf->utf_rsp = tf->tf_rsp;

		//user_mem_assert(curenv,(void*)utf,1,PTE_U);

		//make it so that we start executing at the user pagefault handler,
		//with the user exception stack.
		tf->tf_rip = (uintptr_t)pgfault_upcall; 
		tf->tf_rsp = (uintptr_t)utf;

		env_run(curenv);		
	}
	else{
	
		// Destroy the environment that caused the fault.
		cprintf("[%08x] user fault va %08x ip %08x\n",
			curenv->env_id, fault_va, tf->tf_rip);
		print_trapframe(tf);
		env_destroy(curenv);
	}
}

