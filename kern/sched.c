#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

void
print_envs(void){
	int i = 0;
	struct Env* e;
	for (; i <NENV; i++){
		e = &envs[i];
		if (e->env_status)
			cprintf("env %d, status: %d, cpunum: %d, env_id: %d\n",i,e->env_status,e->env_cpunum,e->env_id);
	}
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.
/*
	Your next task in this lab is to change the JOS kernel so that 
	it can alternate between multiple environments in "round-robin" 
	fashion. Round-robin scheduling in JOS works as follows:

	The function sched_yield() in the new kern/sched.c is responsible for selecting 
	a new environment to run. It searches sequentially through the envs[] array in 
	circular fashion, starting just after the previously running environment (or at 
	the beginning of the array if there was no previously running environment), picks 
	the first environment it finds with a status of ENV_RUNNABLE (see inc/env.h), and 
	calls env_run() to jump into that environment.

	sched_yield() must never run the same environment on two CPUs at the same time. It can tell 
	that an environment is currently running on some CPU (possibly the current CPU) because that 
	environment's status will be ENV_RUNNING.
	We have implemented a new system call for you, sys_yield(), which user environments can call 
	to invoke the kernel's sched_yield() function and thereby 
	voluntarily give up the CPU to a different environment.*/

	// LAB 4: Your code here.	
		
	struct Env* e;
	int i,env_num = 0;
	if (curenv){
		env_num = ENVX(curenv->env_id) + 1;
	}
	//cprintf("printing envs\n");
	//print_envs();
	for (i = 0; i < NENV; i++){
		//circular iteration
		e = &envs[(i+env_num) % NENV];
		if (e->env_status == ENV_RUNNABLE){

			env_run(e);	
		}
	}
	if (thiscpu->cpu_env && (thiscpu->cpu_env->env_status == ENV_RUNNING)){
		env_run(thiscpu->cpu_env);
	}
	// sched_halt never returns
	//cprintf("printing envs:\n");
	//print_envs();
	sched_halt();
}



// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		cprintf("For CPU%d\n",cpunum());
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(boot_pml4));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel	
	unlock_kernel();

	
	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movq $0, %%rbp\n"
		"movq %0, %%rsp\n"
		"pushq $0\n"
		"pushq $0\n"
		"sti\n"
		"hlt\n"
		: : "a" (thiscpu->cpu_ts.ts_esp0));
}

