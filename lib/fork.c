// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	addr = (void*)ROUNDDOWN(addr,PGSIZE);
	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	if (err & FEC_WR){
		if (uvpt[VPN(addr)] & PTE_COW){
			if (sys_page_alloc(0,PFTEMP,PTE_W | PTE_U | PTE_P)){
				panic("sys_page_alloc failed\n");
			}
			memcpy((void*)PFTEMP,addr,PGSIZE);
			if (sys_page_map(0,PFTEMP,0,addr,PTE_W | PTE_U | PTE_P)){
				panic("sys_page_map failed\n");
			}

			if (sys_page_unmap(0,(void*)PFTEMP)){
				panic ("sys_page_unmap failed\n");
			}
		}
		else {
			panic("pgfault VPN(addr) not PTE_COW\n");
		}
	}
	else {
		panic("pgfault err not PTE_W\n");
	}
	
	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void* addr = (void*)((uintptr_t)pn * PGSIZE);

	if (uvpt[pn] & PTE_SHARE){
		unsigned int perm = uvpt[pn] & (PTE_SYSCALL | PTE_SHARE);
		r = sys_page_map(0,addr,envid,addr,perm);
		if (r < 0)
			panic("sys_page_map in duppage failed\n");
		return 0;
	}
		
	if (uvpt[pn] & PTE_W || uvpt[pn] & PTE_COW){
		unsigned int perm = PTE_COW | PTE_U | PTE_P;
		r = sys_page_map(0,addr,envid,addr,perm);
		if (r < 0)
			panic("sys_page_map1 in duppage failed\n");

		
		r = sys_page_map(0,addr,0,addr,perm);
		if (r < 0)
			panic("sys_page_map2 in duppage failed\n");

		return 0;

	}
	else {
		unsigned int perm = PTE_U | PTE_P;
		r = sys_page_map(0,addr,envid,addr,perm);
		if (r < 0)
			panic("sys_page_map3 in duppage failed\n");
		return 0;
	}
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
/*
 The basic control flow for fork() is as follows:

    The parent installs pgfault() as the C-level page fault handler, using the set_pgfault_handler() function you implemented above.
    The parent calls sys_exofork() to create a child environment.
    For each writable or copy-on-write page in its address space below UTOP, the parent calls duppage, which should map the page copy-on-write into the address space of the child and then remap the page copy-on-write in its own address space. duppage sets both PTEs so that the page is not writeable, and to contain PTE_COW in the "avail" field to distinguish copy-on-write pages from genuine read-only pages.

    The exception stack is not remapped this way, however. Instead, you need to allocate a fresh page in the child for the exception stack. Since the page fault handler will be doing the actual copying and the page fault handler runs on the exception stack, the exception stack cannot be made copy-on-write: who would copy it?

    fork() also needs to handle pages that are present, but not writable or copy-on-write.
    The parent sets the user page fault entrypoint for the child to look like its own.
    The child is now ready to run, so the parent marks it runnable.

*/
envid_t
fork(void)
{
	
	set_pgfault_handler(pgfault);
	envid_t child_envid = sys_exofork();
	if (child_envid){
		//called inside parent

		
		//user address space is below UTOP, so we only need PML4[0].
		uint64_t addr = 0;

		//even though I think user mode can potentially have mappings 
		//above the UXSTACKTOP, I think for the lab its safe and easier
		//to keep the bound at USTACKTOP.
		for (; addr < USTACKTOP;){
			if (uvpde[VPDPE(addr)] & PTE_P){
				if (uvpd[VPD(addr)] & PTE_P){
					if (uvpt[VPN(addr)] & PTE_P && uvpt[VPN(addr)] & PTE_U){
						duppage(child_envid, VPN(addr));
					}
					addr += PGSIZE;	
				}
				//we will discover a PDE not present is at 
				//a multiple of PTSIZE;
				else {
					addr += PTSIZE;
				}
			}
			//we will discover a PDPE not present is at
			//a multiple of PTSIZE * NPDENTRIES;	
			else {
				addr += PTSIZE * NPDENTRIES;
			}
		}
		

		//map a user exception stack for the child
		sys_page_alloc(child_envid,(void*)(UXSTACKTOP-PGSIZE),PTE_U | PTE_W| PTE_P);

		//set the pgfault upcall
		extern void* _pgfault_upcall();
		sys_env_set_pgfault_upcall(child_envid,_pgfault_upcall);

		if(sys_env_set_status(child_envid,ENV_RUNNABLE)){
			panic("sys_env_set_status failed\n");
		}		

		// return the childs envid to the parent	
		return child_envid;
	}
	else {
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
