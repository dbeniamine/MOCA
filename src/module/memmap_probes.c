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
//#define MEMMAP_DEBUG

#include <linux/kprobes.h>
#include "memmap.h"
#include "memmap_tasks.h"
#include "memmap_taskdata.h"
#include "memmap_probes.h"
#include "memmap_threads.h"
#include "memmap_page.h"

int MemMap_ExecHandler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    MEMMAP_DEBUG_PRINT("MemMap in exec handler task %p %s %d\n",current,
            current->comm, current->pid);
    //MemMap_AddTaskIfNeeded(current);
    return 0;
}

void MemMap_MmFaultHandler(struct mm_struct *mm, struct vm_area_struct *vma,
        unsigned long address, unsigned int flags)
{
    pte_t *pte;

    task_data data;
    memmap_task tsk;
    MEMMAP_DEBUG_PRINT("Pte fault task %p\n", current);
    if(!(data=MemMap_GetData(current)))
    {
        if(!(tsk=MemMap_AddTaskIfNeeded(current)))
            jprobe_return();
        data=tsk->data;
    }
    MemMap_AddToChunk(data,(void *)address,get_cpu());
    pte=MemMap_PteFromAdress(address,mm);
    //If pte exists, try to fix false pagefault
    if (pte && !pte_none(*pte) && !pte_present(*pte) && !pte_special(*pte))
    {
        /* *pte = pte_set_flags(*pte, _PAGE_PRESENT); */
        MEMMAP_DEBUG_PRINT("MemMap fixing fake pagefault\n");
    }
    MemMap_UpdateClock(get_cpu());
    jprobe_return();

}


/* static struct kretprobe MemMap_ExecProbe = { */
/*     .handler = MemMap_ExecHandler, */
/*     .kp.symbol_name = "do_execve", */
/* }; */

static struct jprobe MemMap_PteFaultjprobe = {
    .entry = MemMap_MmFaultHandler,
    .kp.symbol_name = "handle_mm_fault",
};
int MemMap_RegisterProbes(void)
{
    int ret;
    /* if ((ret=register_kretprobe(&MemMap_ExecProbe))) */
    /*     MemMap_Panic("Unable to register fork probe"); */
    if ((ret=register_jprobe(&MemMap_PteFaultjprobe)))
        MemMap_Panic("Unable to register pte fault probe");
    return ret;
}


void MemMap_UnregisterProbes(void)
{
    /* unregister_kretprobe(&MemMap_ExecProbe); */
    unregister_jprobe(&MemMap_PteFaultjprobe);
}
