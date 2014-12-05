/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * MemMap is a kernel module designed to track memory access
 *
 * Copyright (C) 2010 David Beniamine
 * Author: David Beniamine <David.Beniamine@imag.fr>
 */

#include <linux/kprobes.h>
#include "memmap.h"
#include "memmap_tasks.h"
#include "memmap_taskdata.h"
#include "memmap_probes.h"

int MemMap_ForkHandler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    /* MEMMAP_DEBUG_PRINT("MemMap in fork handler pid %lu \n",regs_return_value(regs)); */
    MemMap_AddTaskIfNeeded(regs_return_value(regs));
    return 0;
}

void MemMap_PteFaultHandler(struct mm_struct *mm,
        struct vm_area_struct *vma, unsigned long address,
        pte_t *pte, pmd_t *pmd, unsigned int flags)
{
    task_data data;
    data=MemMap_GetData(mm->owner);
    if(!data || MemMap_GetTaskFromData(data)!=mm->owner)
        jprobe_return();
    // Add pte to current chunk
    MemMap_LockData(data);
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pte fault %p data %p\n",pte, data);
    MemMap_AddToChunk(data,(void *)pte,get_cpu(),MemMap_CurrentChunk(data));
    //Do false pagefault if needed
    /* if(MemMap_IsInChunk(data,(void *)pte,MemMap_PreviousChunk(data))) */
    /* { */
    /* MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pte %p in prev chunk for data %p\n", */
            /* pte, data); */
    if (!pte_none(*pte) && !pte_present(*pte) && !pte_special(*pte))
    {
        *pte = pte_set_flags(*pte, _PAGE_PRESENT);
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap fixing fake pagefault\n");
    }
    /* } */
    MemMap_unLockData(data);
    jprobe_return();
}


//Note that the probes is on the do_fork function which is called also for
//thread creation.
static struct kretprobe MemMap_ForkProbe = {
    .handler = MemMap_ForkHandler,
    .kp.symbol_name = "do_fork",
};

//BUG: since linux 3.3 (I think) handle_pte_fault is not available in
///proc/kallsyms
static struct jprobe MemMap_PteFaultjprobe = {
    .entry = MemMap_PteFaultHandler,
    .kp.symbol_name = "handle_pte_fault",
};
int MemMap_RegisterProbes(void)
{
    int ret;
    if ((ret=register_kretprobe(&MemMap_ForkProbe)))
        MemMap_Panic("Unable to register fork probe");
    if ((ret=register_jprobe(&MemMap_PteFaultjprobe)))
        MemMap_Panic("Unable to register pte fault probe");
    return ret;
}


void MemMap_UnregisterProbes(void)
{
    unregister_kretprobe(&MemMap_ForkProbe);
    unregister_jprobe(&MemMap_PteFaultjprobe);
}
