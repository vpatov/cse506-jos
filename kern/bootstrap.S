#include <inc/mmu.h>
#include <inc/memlayout.h>


# Translation: cr3->pml4->pdpe->pgdir
# We set the PTE_MBZ bit in pgdir to skip the last step	
.code32
.set pml4, pml4phys
.set pdp1, pml4 + 0x1000
.set pdp2, pml4 + 2*0x1000
.set pd1,  pml4 + 3*0x1000
.set pd2,  pml4 + 4*0x1000

.text
.globl _head64
_head64:

# Save multiboot_info addr passed by bootloader
	
    movl $multiboot_info, %eax
    movl %ebx, (%eax)

    movw $0x1234,0x472			# warm boot	
	
# Reset the stack pointer in case we didn't come from the loader
    movl $0x7c00, %esp

    call verify_cpu   #check if CPU supports long mode
    movl $CR4_PAE, %eax	
    movl %eax, %cr4

# build an early boot pml4 at physical address pml4phys 

    #initializing the page tables
    movl $pml4, %edi
    xorl %eax, %eax
    movl $((4096/4)*5), %ecx  # moving these many words to the 6 pages with 4 second level pages + 1 3rd level + 1 4th level pages 
    rep stosl

    # creating an initial page table to map the first 256 MB of physical memory
    # need two PDPs
    movl $pml4, %eax
    movl $pdp1, %ebx
    orl $PTE_P, %ebx
    orl $PTE_W, %ebx
    movl %ebx, (%eax)

    movl $pdp2, %ebx
    orl $PTE_P, %ebx
    orl $PTE_W, %ebx
    movl %ebx, 0x8(%eax)

    # need two PDs, one per PDP
    movl $pdp1, %edi
    movl $pd1, %ebx
    orl $PTE_P, %ebx
    orl $PTE_W, %ebx
    movl %ebx,(%edi)

    movl $pdp2, %edi
    movl $pd2, %ebx
    orl $PTE_P, %ebx
    orl $PTE_W, %ebx
    movl %ebx,(%edi)
    
    # setting the pgdirs so that:
    # 1) LA=PA for the first 256MB of the mem
    # 2) also mapping first 256MB of mem at KERNBASE
    movl $128, %ecx
    movl $pd1, %edi
    # entry 32 of pd2 is 0x8004000000
    movl $pd2, %ebx
    addl $(32*8), %ebx
    # PTE_P|PTE_W|PTE_MBZ
    movl $0x00000183, %eax
1:
    movl %eax, (%edi)
    movl %eax, (%ebx)
    addl $0x8, %edi
    addl $0x8, %ebx
    addl $0x00200000, %eax
    subl $1, %ecx
    cmp $0x0, %ecx
    jne 1b

    # set the cr3 register
    movl $pml4, %eax
    movl %eax, %cr3
	
    # enable the long mode in MSR
    movl $EFER_MSR, %ecx
    rdmsr
    btsl $EFER_LME, %eax
    wrmsr
    
    # enable paging 
    movl %cr0, %eax
    orl $CR0_PE, %eax
    orl $CR0_PG, %eax
    orl $CR0_AM, %eax
    orl $CR0_WP, %eax
    orl $CR0_MP, %eax
    movl %eax, %cr0

    # switch to long mode using lret
    # should first push CS and "return address" (_start) on the stack
    movl $gdtdesc_64, %eax
    lgdt (%eax)
    pushl $0x8
    movl $_start, %eax
    pushl %eax
    
.globl jumpto_longmode
.type jumpto_longmode,@function
jumpto_longmode:
    lret

verify_cpu:
    pushfl                   # get eflags in eax -- standardard way to check for cpuid
    popl %eax
    movl %eax, %ecx
    xorl $0x200000, %eax
    pushl %eax
    popfl
    pushfl
    popl %eax
    cmpl %eax, %ecx
    jz verify_cpu_no_longmode   # no cpuid -- no long mode

    movl $0x0, %eax              # see if cpuid 1 is implemented
    cpuid
    cmpl $0x1, %eax
    jb verify_cpu_no_longmode    # cpuid 1 is not implemented

    mov $0x80000001, %eax
    cpuid                 
    test $(1 << 29), %edx                 #Test if the LM-bit, is set or not.
    jz verify_cpu_no_longmode

    ret

verify_cpu_no_longmode:
    jmp verify_cpu_no_longmode
 
   
.data 

.p2align 2
gdt_64:
    SEG_NULL
    .quad  0x00af9a000000ffff            #64 bit CS
    .quad  0x00cf92000000ffff            #64 bit DS

gdtdesc_64:
    .word 0x17 
    .quad gdt_64

.p2align PGSHIFT
.globl pml4phys
pml4phys:
    .space 5*PGSIZE

.globl multiboot_info
multiboot_info:
    .long 0

