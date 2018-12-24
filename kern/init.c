/* See COPYRIGHT for copyright information. */

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/memlayout.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/kdebug.h>
#include <kern/dwarf_api.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/time.h>
#include <kern/pci.h>

uint64_t end_debug;

static void boot_aps(void);
int enable_sse(void);


void
i386_init(void)
{
	/* __asm __volatile("int $12"); */

	extern char edata[], end[];

	// Before doing anything else, complete the ELF loading process.
	// Clear the uninitialized global data (BSS) section of our program.
	// This ensures that all static/global variables start out zero.
	memset(edata, 0, end - edata);

	// Initialize the console.
	// Can't call cprintf until after we do this!
	cons_init();

	cprintf("6828 decimal is %o octal!\n", 6828);

	extern char end[];
	end_debug = read_section_headers((0x10000+KERNBASE), (uintptr_t)end);

	enable_sse();

	// Lab 2 memory management initialization functions
	x64_vm_init();

	// Lab 3 user environment initialization functions
	env_init();
	trap_init();

	// Lab 4 multiprocessor initialization functions
	mp_init();
	lapic_init();

	// Lab 4 multitasking initialization functions
	pic_init();

	// Lab 6 hardware initialization functions
	time_init();
	pci_init();

	// Acquire the big kernel lock before waking up APs
	// LAB 4: Your code here.

	lock_kernel();
	// Starting non-boot CPUs
	boot_aps();
	



	// Start fs.
	ENV_CREATE(fs_fs, ENV_TYPE_FS);
	

#if !defined(TEST_NO_NS) && !defined(VMM_GUEST)
	// Start ns.
	ENV_CREATE(net_ns, ENV_TYPE_NS);
#endif

#if defined(TEST)
	// Don't touch -- used by grading script!
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	// Touch all you want.
//	ENV_CREATE(user_testfloat,ENV_TYPE_USER);
	ENV_CREATE(user_testshell,ENV_TYPE_USER);
#endif // TEST*

	// Should not be necessary - drains keyboard because interrupt has given up.
	kbd_intr();

	// Schedule and run the first user environment!
	sched_yield();
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// Start the non-boot (AP) processors.
static void
boot_aps(void)
{
	extern unsigned char mpentry_start[], mpentry_end[];
	void *code;
	struct CpuInfo *c;

	// Write entry code to unused memory at MPENTRY_PADDR
	code = KADDR(MPENTRY_PADDR);
	memmove(code, mpentry_start, mpentry_end - mpentry_start);

	// Boot each AP one at a time
	for (c = cpus; c < cpus + ncpu; c++) {
		if (c == cpus + cpunum())  // We've started already.
			continue;

		// Tell mpentry.S what stack to use 
		mpentry_kstack = percpu_kstacks[c - cpus] + KSTKSIZE;
		// Start the CPU at mpentry_start
		lapic_startap(c->cpu_id, PADDR(code));
		// Wait for the CPU to finish some basic setup in mp_main()
		while (c->cpu_status != CPU_STARTED)
			;
	}
}

// Setup code for APs
void
mp_main(void)
{
	// We are in high RIP now, safe to switch to kern_pgdir
	lcr3(boot_cr3);
	cprintf("SMP: CPU %d starting\n", cpunum());

	lapic_init();
	env_init_percpu();
	trap_init_percpu();
	xchg(&thiscpu->cpu_status, CPU_STARTED); // tell boot_aps() we're up

	// Now that we have finished some basic setup, call sched_yield()
	// to start running processes on this CPU.  But make sure that
	// only one CPU can enter the scheduler at a time!
	// LAB 4: Your code here.

	lock_kernel();
	sched_yield();
}

int
enable_sse(void){
	uint32_t eax, ebx, ecx, edx, info, cr4;
	info = 0x1;
	cpuid(info,&eax,&ebx,&ecx,&edx);
	cprintf("eax: %04x, ebx: %04x, ecx: %04x, edx %04x\n",eax,ebx,ecx,edx);
	//check for SSE/SSE2/SSE3
	assert(ecx & (1 << 0));
	assert(edx & (1 << 25));
	assert(edx & (1 << 26));

	//Check for CLFLUSH
	assert(edx & (1 << 19));

	//Get cr4 register, and set the appropriate flags
	cr4 = rcr4();

/*	Set the OSFXSR flag (bit 9 in control register CR4) to 
	indicate that the operating system supports saving and
	restoring the SSE/SSE2/SSE3/SSSE3 execution environment
	(XMM and MXCSR registers) with the FXSAVE and
	FXRSTOR instructions, respectively. (From intel manual)

	Set the OSXMMEXCPT flag (bit 10 in control register CR4) 
	to indicate that the operating system supports the
	handling of SSE/SSE2/SSE3 SIMD floating-point exceptions (#XM).
	See Section 2.5, “Control Registers,” for a
	description of the OSXMMEXCPT flag.	
*/
	//set the 9th and 10th bit in the cr register.
	cr4 |= (1 << 9) | (1 << 10);
	//apply
	lcr4(cr4);

	
	return 0;
}

/*
 * Variable panicstr contains argument to first call to panic; used as flag
 * to indicate that the kernel has already called panic.
 */
const char *panicstr;

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then enters the kernel monitor.
 */
void
_panic(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	if (panicstr)
		goto dead;
	panicstr = fmt;

	// Be extra sure that the machine is in as reasonable state
	__asm __volatile("cli; cld");

	va_start(ap, fmt);
	cprintf("kernel panic on CPU %d at %s:%d: ", cpunum(), file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);

dead:
	/* break into the kernel monitor */
	while (1)
		monitor(NULL);
}

/* like panic, but don't */
void
_warn(const char *file, int line, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
