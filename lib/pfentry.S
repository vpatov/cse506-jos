#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <kern/macro.h>

// Page fault upcall entrypoint.

// This is where we ask the kernel to redirect us to whenever we cause
// a page fault in user space (see the call to sys_set_pgfault_handler
// in pgfault.c).
//
// When a page fault actually occurs, the kernel switches our ESP to
// point to the user exception stack if we're not already on the user
// exception stack, and then it pushes a UTrapframe onto our user
// exception stack:
//
//	trap-time esp
//	trap-time eflags
//	trap-time eip
//	utf_regs.reg_rax
//	...
//	utf_regs.reg_rsi
//	utf_regs.reg_rdi
//	utf_err (error code)
//	utf_fault_va            <-- %esp
//
// If this is a recursive fault, the kernel will reserve for us a
// blank word above the trap-time esp for scratch work when we unwind
// the recursive call.
//
// We then have call up to the appropriate page fault handler in C
// code, pointed to by the global variable '_pgfault_handler'.

.text
.globl _pgfault_upcall
_pgfault_upcall:
// Call the C page fault handler.
// function argument: pointer to UTF

movq  %rsp,%rdi                // passing the function argument in rdi
movabs _pgfault_handler, %rax
call *%rax

// Now the C page fault handler has returned and you must return
// to the trap time state.
// Push trap-time %eip onto the trap-time stack.
//
// Explanation:
//   We must prepare the trap-time stack for our eventual return to
//   re-execute the instruction that faulted.
//   Unfortunately, we can't return directly from the exception stack:
//   We can't call 'jmp', since that requires that we load the address
//   into a register, and all registers must have their trap-time
//   values after the return.
//   We can't call 'ret' from the exception stack either, since if we
//   did, %esp would have the wrong value.
//   So instead, we push the trap-time %eip onto the *trap-time* stack!
//   Below we'll switch to that stack and call 'ret', which will
//   restore %eip to its pre-fault value.
//
//   In the case of a recursive fault on the exception stack,
//   note that the word we're pushing now will fit in the
//   blank word that the kernel reserved for us.
//
// Throughout the remaining code, think carefully about what
// registers are available for intermediate calculations.  You
// may find that you have to rearrange your code in non-obvious
// ways as registers become unavailable as scratch space.
//
// LAB 4: Your code here.

    // Restore the trap-time registers.  After you do this, you
    // can no longer modify any general-purpose registers.
    // LAB 4: Your code here.

    // Restore eflags from the stack.  After you do this, you can
    // no longer use arithmetic operations or anything else that
    // modifies eflags.
    // LAB 4: Your code here.

    // Switch back to the adjusted trap-time stack.
    // LAB 4: Your code here.

    // Return to re-execute the instruction that faulted.
    // LAB 4: Your code here.

	//moving past tf_err and fault_va
	popq %rax;
	popq %rax;

	//regs are (15 * 8) = 120 bytes, eflags is 8, rip is 8, rsp is 8.
	add $136,	%rsp; 	// rsp is pointing to tf_rsp
	mov (%rsp),	%r15;	// tf_rsp is moved into r15
	sub $8,		%r15;	// subtract a word from tf_rsp, so that it is correct once we ret
	mov %r15,	(%rsp);	// move the updated tf_rsp back into tf_rsp
	sub $16,	%rsp;	// rsp is pointing to tf_rip
	mov (%rsp),	%r14;	// mov tf_rip onto into r14
	mov %r14,	(%r15);	// mov tf_rip onto the top of the traptime stack.

	sub $120,	%rsp;	//rsp is now pointing to rax in user trapframe.
	
	POPA_;			//after this, rsp is now pointing to rip again.

	add $8,		%rsp;	//go past rip, to eflags
	popfq;			//pop into eflags
	popq 		%rsp;	//now rsp is pointing to tf_rsp, which we pop into rsp
	ret;			//return

	
