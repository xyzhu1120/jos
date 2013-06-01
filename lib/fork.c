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

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at vpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.
	//   No need to explicitly delete the old page's mapping.

	// LAB 4: Your code here.

	pde_t pde = vpd[PDX(addr)];
	pte_t pte = vpt[PGNUM(addr)];

	if (!(pde & PTE_P))
		panic("pgfault: wrong pde\n");

	if((pte & (PTE_U | PTE_P | PTE_COW))==0)
		panic("fgfault: wrong pte\n");
	r = sys_page_alloc(0, (void *)PFTEMP, PTE_U | PTE_P | PTE_W);
	if(r < 0)
		panic("pgfault: bad sys page alloc");

	memmove((void *) PFTEMP, (void *)((PGNUM(addr))<<PTXSHIFT), PGSIZE);

	r = sys_page_map(0, (void *)PFTEMP, 0, (void *)((PGNUM(addr))<<PTXSHIFT), PTE_U | PTE_P | PTE_W);
	if(r < 0)
		panic("pgfault: bad sys page alloc");
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

	unsigned int addr = pn * PGSIZE;

	if(addr >= UTOP)
		panic("larger than UTOP");

	pde_t pde;
	pde = vpd[PDX(addr)];

	if((pde & PTE_P) == 0) {
		return 0;
	}

	pte_t pte = vpt[PGNUM(addr)];
	if((pte & PTE_P) == 0){
		return 0;
	}

	if((pte & PTE_W) != 0 || (pte & PTE_COW) != 0){
		r = sys_page_map(0, (void *)addr, envid, (void *)addr,  PTE_U|PTE_P|PTE_COW);
		if(r < 0)
			panic("duppage: error page map\n");
		r = sys_page_map(0, (void *)addr, 0, (void *)addr ,PTE_U|PTE_P|PTE_COW);
		if(r < 0)
			panic("duppage: error page map\n");
	}else{
		r = sys_page_map(0, (void *)addr, envid, (void *)addr, PTE_U|PTE_P);
		if(r < 0)
			panic("duppage: error page map\n");
	}
	// LAB 4: Your code here.
	return 0;
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
//   Use vpd, vpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	envid_t envid;
	set_pgfault_handler(pgfault);	
	envid = sys_exofork();
	if(envid < 0) {
		panic("exofork error\n");
	}

	if(envid == 0){
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	int i;
	for(i = 0; i < UTOP/PGSIZE - 1; i++) {
		duppage(envid, i);
	}

	int r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_U | PTE_P | PTE_W);
	if(r < 0)
		panic("sys page alloc failed\n");
	extern void _pgfault_upcall(void);
	r = sys_env_set_pgfault_upcall(envid, (void *)_pgfault_upcall);
	if(r < 0)
		panic("sys env set pgfault upcall failded\n");
	sys_env_set_status(envid, ENV_RUNNABLE);
	return envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
