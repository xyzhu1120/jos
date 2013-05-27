// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

struct Stackframe{
	int ebp;
	int eip;
	int *args;
	int argn;
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display the stack info", mon_backtrace},
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

unsigned read_eip();

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
	extern char entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		(end-entry+1023)/1024);
	return 0;
}


// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

        char str[256] = {0x55,0x89,0xe5};
	int nstr = 0;
        char *pret_addr;
        int eip;
 	int i;
	unsigned int addr;
	for(i = 0; i < 256; i++){
		str[i] = '1';
	}
       
	__asm __volatile("movl %%ebp, %0" : "=a" (pret_addr));
	__asm __volatile("movl 4(%%ebp), %0" : "=a" (eip));
	addr = (unsigned int)str;
	str[addr & 0x000000ff] = '\0';
	pret_addr += 4;
	cprintf("%s%n",str,pret_addr);
	pret_addr += 1;
	str[addr & 0x000000ff] = '1';
	str[(addr >> 8) & 0x000000ff] = '\0';
	cprintf("%s%n",str,pret_addr);
	str[(addr >> 8) & 0x000000ff] = '1';
	str[(addr >> 16) & 0x000000ff] = '\0';
	pret_addr += 1;
	cprintf("%s%n",str,pret_addr);
	str[(addr >> 16) & 0x000000ff] = '1';
	str[(addr >> 24) & 0x000000ff] = '\0';
	pret_addr += 1;
	cprintf("%s%n",str,pret_addr);
	// Your code here.
	str[0] = 0x68;
	*((int *)(str+1)) = eip;
	str[5] = 0x68;
	*((int *)(str+6)) = (int)do_overflow;
	str[10] = 0xc3;
   	cprintf("%s\n",str); 
	cprintf("%x\n",eip);
}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	int i;
	uint32_t ebp;
	uint32_t eip;
	uint32_t ebp2;
	struct Eipdebuginfo di;
	__asm __volatile("movl %%ebp, %0" : "=r" (ebp));
	eip = read_eip();
	//__asm __volatile("movl 4(%%ebp), %0" : "=r" (eip));
	//debuginfo_eip(eip, &di);	
	//cprintf("ebp %x	eip %x	args\n", ebp, eip);	
	//cprintf("\t%s:%d %s+%u\n", di.eip_file, di.eip_line, di.eip_fn_name, di.eip_fn_addr);
	do {
		cprintf("ebp %x	eip %x	args %08x %08x %08x %08x %08x\n", ebp, eip, *(((int *)ebp)+2),*(((int *)ebp)+3),*(((int *)ebp)+4),*(((int *)ebp)+5),*(((int *)ebp)+6));
		__asm __volatile("movl (%1), %0" : "=q" (ebp2):"q" (ebp));
		__asm __volatile("movl 4(%1), %0" : "=q" (eip): "q" (ebp));
		debuginfo_eip(eip, &di);	
		ebp = ebp2;
		cprintf("\t%s:%d %s+%u\n", di.eip_file, di.eip_line, di.eip_fn_name, eip - di.eip_fn_addr);
		
	} while(ebp != 0);
        overflow_me();
        cprintf("Backtrace success\n");
	return 0;
}
//int
//mon_backtrace(int argc, char **argv, struct Trapframe *tf)
//{
//	// Your code here.
//    overflow_me();
//    cprintf("Backtrace success\n");
//	return 0;
//}



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

// return EIP of caller.
// does not work if inlined.
// putting at the end of the file seems to prevent inlining.
unsigned
read_eip()
{
	uint32_t callerpc;
	__asm __volatile("movl 4(%%ebp), %0" : "=r" (callerpc));
	return callerpc;
}
