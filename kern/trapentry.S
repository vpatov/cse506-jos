/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <kern/macro.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;			/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;				/* align function definition */		\
	name:					/* function starts here */		\
	cli;								\
	pushq $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	cli;								\
	pushq $0;							\
	pushq $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
   Find the PUSHA, POPA macros in kern/macro.h. Since amd64 doesn't support
   pusha, popa so you will find these macros handy.
*/

TRAPHANDLER_NOEC(trph0,0);
TRAPHANDLER_NOEC(trph1,1);
TRAPHANDLER_NOEC(trph2,2);
TRAPHANDLER_NOEC(trph3,3);
TRAPHANDLER_NOEC(trph4,4);
TRAPHANDLER_NOEC(trph5,5);
TRAPHANDLER_NOEC(trph6,6);
TRAPHANDLER_NOEC(trph7,7);
TRAPHANDLER(trph8,8);
//TRAPHANDLER_NOEC(trph__coproc,9);	//reserved
TRAPHANDLER(trph10,10);
TRAPHANDLER(trph11,11);
TRAPHANDLER(trph12,12);
TRAPHANDLER(trph13,13);
TRAPHANDLER(trph14,14);
//TRAPHANDLER_NOEC(trph__res,15); 	//reserved
TRAPHANDLER_NOEC(trph16,16);
TRAPHANDLER_NOEC(trph17,17);
TRAPHANDLER_NOEC(trph18,18);
TRAPHANDLER_NOEC(trph19,19);
TRAPHANDLER_NOEC(trph48,48);
TRAPHANDLER_NOEC(trph_default,500);

TRAPHANDLER_NOEC(irq0,IRQ_OFFSET + 0);
TRAPHANDLER_NOEC(irq1,IRQ_OFFSET + IRQ_KBD);
TRAPHANDLER_NOEC(irq2,IRQ_OFFSET + IRQ_SERIAL);
/*
 * Lab 3: Your code here for _alltraps
 *
 * Hint: Be sure to review the x64 calling convention from lab1
 *       for how to pass a pointer to the trapframe.
 */

//push values to make the stack look like a struct Trapframe
//load GD_KD into %ds and %es
//Pass a pointer to the Trapframe as an argument to trap() (Hint: Review the x64 calling convention from lab 1).
//call trap (can trap ever return?)

_alltraps:


	//pushq %ds; //even though these are only 16-bits, our trapframe has padding.
	//pushq %es; //possible bug: when we pushq, will the struct Trapframe have the 16 bits in the right spot? (and not in the padding?)

	//PUSHA; //push all the registers
	
	//movw $GD_KD %ds;


	//cant use pushq on a segment register, but the registers %ds and %es have to be pushed
	//first in order to preserve the struct Trapframe. So, we could move %ds into %rax first, and then push,
	// but the problem with that is we will overwrrite %rax, and we are going to push that soon anyway.
	//a workaround is to use sub and mov, just like in PUSHA

	cli;
	sub $16, %rsp;
	movw %ds, 8(%rsp);
	movw %es, 0(%rsp);

	PUSHA;
	movw $GD_KD, %rax;
	movw %rax, %ds;
	movw %rax, %es;

	
	movq %rsp, %rdi;	
	call trap
	


