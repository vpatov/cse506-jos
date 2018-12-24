// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/dwarf.h>
#include <kern/kdebug.h>
#include <kern/dwarf_api.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "monbacktrace","Display a backtrace of execution (might be broken)", mon_backtrace},
	{ "colortest", "Display demonstration of printing with colors",colortest},
	{" shutdown", "Shutdown the computer", mon_shutdown},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint64_t *next_rbp;
	uint64_t *next_rip;
	struct Ripdebuginfo first_info;
	uint64_t* first_rip;
	
	const char *long_format = "%016x ";
	const char *int_format = "%08x ";
	const char *char_format = "%02x ";

	next_rbp = (uint64_t*)(read_rbp());
	read_rip(first_rip);	
	debuginfo_rip((uintptr_t)first_rip,&first_info);

	cprintf("rbp %016x rip %016x\n",next_rbp,first_rip);

	cprintf("%s:%d: ",
	first_info.rip_file,
	first_info.rip_line);

	cprintf("%.*s",first_info.rip_fn_namelen,first_info.rip_fn_name);


	//figuring out that these values below needed to be cast took me longer to
	//realize than I'd like to admit.
	cprintf("+%016x ", (uint64_t)first_rip - ((uint64_t)first_info.rip_fn_addr));


	cprintf("args:%d ",first_info.rip_fn_narg);
	
	int i = 0;
	for (; i < first_info.rip_fn_narg; i++){
		const char *format;
		switch (first_info.size_fn_arg[i]){
		case 8: format = long_format; break;
		case 4: format = int_format; break;
		case 1: format = char_format; break;
		default: format = long_format;
		}
		cprintf(format, *((uint64_t*) (((uint64_t)next_rbp + 
		first_info.reg_table.cfa_rule.dw_offset + (uint64_t)first_info.offset_fn_arg[i]))) );
	}
	cprintf("\n");
	
	next_rip = (uint64_t*)(next_rbp) + 1;
	next_rbp = (uint64_t*)(*next_rbp);

	while(next_rbp != 0){

		cprintf("rbp %016x rip %016x\n",next_rbp,*next_rip);
		

		struct Ripdebuginfo info;
		debuginfo_rip(*next_rip,&info);

		cprintf("%s:%d: ",
		info.rip_file,
		info.rip_line);

		cprintf("%.*s",info.rip_fn_namelen,info.rip_fn_name);
		
		cprintf("+%016x ", *next_rip - info.rip_fn_addr);


		cprintf("args:%d ",info.rip_fn_narg);


		int i = 0;
        	for (; i < info.rip_fn_narg; i++){
                	const char *format2;
			switch (info.size_fn_arg[i]){
				case 8: format2 = long_format; 
				cprintf(format2, *((uint64_t*) (((uint64_t)next_rbp + info.reg_table.cfa_rule.dw_offset + (uint64_t)info.offset_fn_arg[i]))));
				break;
				case 4: format2 = int_format; 
				cprintf(long_format, *((uint32_t*) (((uint64_t)next_rbp + info.reg_table.cfa_rule.dw_offset + (uint64_t)info.offset_fn_arg[i]))));	
				break;
				case 1: format2 = char_format; 
				cprintf(format2, *((char*) (((uint64_t)next_rbp + info.reg_table.cfa_rule.dw_offset + (uint64_t)info.offset_fn_arg[i]))));
				break;
				default: format2 = long_format; cprintf("defaulted in switch: %u", info.size_fn_arg[i]);
				cprintf(format2, *((uint64_t*) (((uint64_t)next_rbp + info.reg_table.cfa_rule.dw_offset + (uint64_t)info.offset_fn_arg[i]))));
				
			}
			//cprintf(format2, *((uint64_t*) (((uint64_t)next_rbp + info.reg_table.cfa_rule.dw_offset + (uint64_t)info.offset_fn_arg[i]))));
        	}
        	cprintf("\n");

		next_rip = ((uint64_t*)(next_rbp)) + 1;
		next_rbp = (uint64_t*)(*next_rbp);
	}

	
	return 0;


}

int mon_shutdown(int argc, char** argv, struct Trapframe *tf){
	return 0;
}

int colortest(int argc, char** argv, struct Trapframe *tf){

	const char* test_string = "Behold these beautiful 4-bit colors!";

	
	
	cprintf("%r0\n",test_string);
	cprintf("%r1\n",test_string);
	cprintf("%r2\n",test_string);
	cprintf("%r3\n",test_string);
	cprintf("%r4\n",test_string);
	cprintf("%r5\n",test_string);
	cprintf("%r6\n",test_string);
	cprintf("%r7\n",test_string);
	cprintf("%r8\n",test_string);
	cprintf("%r9\n",test_string);
	cprintf("%ra\n",test_string);
	cprintf("%rb\n",test_string);
	cprintf("%rc\n",test_string);
	cprintf("%rd\n",test_string);
	cprintf("%re\n",test_string);
	cprintf("%rf\n",test_string);




	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
