/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Moca is a kernel module designed to track memory access
 *
 * Copyright (C) 2010 David Beniamine
 * Author: David Beniamine <David.Beniamine@imag.fr>
 */
#define __NO_VERSION__
/* #define MOCA_DEBUG */

#include <linux/kprobes.h>
#include <linux/delay.h> //msleep
#include "moca.h"
#include "moca_tasks.h"
#include "moca_taskdata.h"
#include "moca_probes.h"
#include "moca_page.h"
#include "moca_false_pf.h"


void Moca_MmFaultHandler(struct mm_struct *mm, struct vm_area_struct *vma,
        unsigned long address, unsigned int flags)
{
    task_data data;
    moca_task tsk;
    if(!(data=Moca_GetData(current)))
    {
        if(!Moca_IsActivated() || !(tsk=Moca_AddTaskIfNeeded(current)))
            jprobe_return();
        data=tsk->data;
    }
    MOCA_DEBUG_PRINT("Moca Pte fault task %p\n", current);
    Moca_UpdateClock();
    Moca_RLockPf();
    if(Moca_IsActivated())
        Moca_AddToChunk(data,(void *)address,get_cpu());
    if(Moca_FixFalsePf(mm,address)!=0)
        MOCA_DEBUG_PRINT("Moca true fault at a: %p a&m: %p mm: %p\n",
                (void *)address, (void *)(address&PAGE_MASK),mm);
    jprobe_return();
}

void Moca_ExitHandler(struct mmu_gather *tlb, struct vm_area_struct *start_vma,
		unsigned long start, unsigned long end)
{
    struct mm_struct *mm=start_vma->vm_mm;
    if(current && Moca_GetData(current)!=NULL)
    {
        MOCA_DEBUG_PRINT("Moca Exit handler handler task %p, mm %p\n", current, mm);
        Moca_RLockPf();
        Moca_FixAllFalsePf(mm);
    }
    jprobe_return();
}

static int Moca_HandlerPost(struct kretprobe_instance *ri,
        struct pt_regs *regs)
{
    if(current && Moca_GetData(current)!=NULL)
    {
        MOCA_DEBUG_PRINT("Moca return probe task %p mm %p\n",
                current, current->mm);
        Moca_RUnlockPf();
        MOCA_DEBUG_PRINT("Moca return probe unlocked task %p mm %p\n",
                current, current->mm);
    }
    return 0;
}

static struct jprobe Moca_PteFaultjprobe = {
    .entry = Moca_MmFaultHandler,
    .kp.symbol_name = "handle_mm_fault",
};

static struct kretprobe Moca_PostFaultProbe = {
    .handler=Moca_HandlerPost,
    .maxactive=2*NR_CPUS,
};

static struct jprobe Moca_ExitProbe = {
    .entry=Moca_ExitHandler,
    .kp.symbol_name="unmap_vmas",
};

static struct kretprobe Moca_PostExitProbe = {
    .handler=Moca_HandlerPost,
    .maxactive=2*NR_CPUS,
};

// Debug handler not used in general case
static int Moca_ProbeFaultHandler(struct kprobe *p, struct pt_regs *regs,
        int trapnr)
{
	printk(KERN_INFO "MOCA fault_handler: p->addr = 0x%p, symbole: %s trap #%dn task %p\n",
		p->addr, p->symbol_name,trapnr,current);
#ifdef CONFIG_X86
	printk(KERN_INFO "pre_handler: p->addr = 0x%p, ip = %lx,"
			" flags = 0x%lx\n",
		p->addr, regs->ip, regs->flags);
#endif
#ifdef CONFIG_PPC
	printk(KERN_INFO "pre_handler: p->addr = 0x%p, nip = 0x%lx,"
			" msr = 0x%lx\n",
		p->addr, regs->nip, regs->msr);
#endif
#ifdef CONFIG_MIPS
	printk(KERN_INFO "pre_handler: p->addr = 0x%p, epc = 0x%lx,"
			" status = 0x%lx\n",
		p->addr, regs->cp0_epc, regs->cp0_status);
#endif
#ifdef CONFIG_TILEGX
	printk(KERN_INFO "pre_handler: p->addr = 0x%p, pc = 0x%lx,"
			" ex1 = 0x%lx\n",
		p->addr, regs->pc, regs->ex1);
#endif
    dump_stack();
    if(p==&Moca_PteFaultjprobe.kp || p==&Moca_ExitProbe.kp)
        Moca_HandlerPost(NULL,NULL);//Release locks if any
	/* Return 0 because we don't handle the fault. */
	return 0;
}


int Moca_RegisterProbes(void)
{
    int ret;
    MOCA_DEBUG_PRINT("Moca registering probes\n");

    Moca_PostExitProbe.kp.symbol_name="unmap_vmas";
	Moca_PostExitProbe.kp.fault_handler = Moca_ProbeFaultHandler;
    if ((ret=register_kretprobe(&Moca_PostExitProbe)))
        Moca_Panic("Unable to register post exit probe");


	Moca_ExitProbe.kp.fault_handler = Moca_ProbeFaultHandler;
    if ((ret=register_jprobe(&Moca_ExitProbe)))
        Moca_Panic("Unable to register do exit probe");

    Moca_PostFaultProbe.kp.symbol_name = "handle_mm_fault";
	Moca_PostFaultProbe.kp.fault_handler = Moca_ProbeFaultHandler;
    if ((ret=register_kretprobe(&Moca_PostFaultProbe)))
        Moca_Panic("Unable to register post fault probe");

	Moca_PteFaultjprobe.kp.fault_handler = Moca_ProbeFaultHandler;
    if ((ret=register_jprobe(&Moca_PteFaultjprobe)))
        Moca_Panic("Moca Unable to register pte fault probe");

    MOCA_DEBUG_PRINT("Moca registered probes\n");
    return ret;
}


void Moca_UnregisterProbes(void)
{
    unregister_jprobe(&Moca_PteFaultjprobe);
    unregister_jprobe(&Moca_ExitProbe);
    // might need a delay here
    msleep(2*MOCA_DEFAULT_WAKEUP_INTERVAL);
    unregister_kretprobe(&Moca_PostFaultProbe);
    unregister_kretprobe(&Moca_PostExitProbe);
}
