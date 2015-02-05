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
#include "moca.h"
#include "moca_tasks.h"
#include "moca_taskdata.h"
#include "moca_probes.h"
#include "moca_page.h"
#include "moca_false_pf.h"


void Moca_MmFaultHandler(struct mm_struct *mm, struct vm_area_struct *vma,
        unsigned long address, unsigned int flags)
{
    pte_t *pte;

    task_data data;
    moca_task tsk;
    if(!(data=Moca_GetData(current)))
    {
        if(!(tsk=Moca_AddTaskIfNeeded(current)))
            jprobe_return();
        data=tsk->data;
    }
    MOCA_DEBUG_PRINT("Moca Pte fault task %p\n", current);
    pte=Moca_PteFromAdress(address,mm);
    if(pte && !pte_none(*pte))
        /* Moca_FixFalsePf(mm,pte); */
        *pte=pte_set_flags(*pte, _PAGE_PRESENT);
    Moca_AddToChunk(data,(void *)(address&PAGE_MASK),get_cpu());
    Moca_UpdateClock();
    jprobe_return();

}

/* void Moca_UnmapPageHandler(struct mmu_gather *tlb, */
/*         struct vm_area_struct *vma, */
/*         unsigned long addr, unsigned long end, */
/*         struct zap_details *details) */
/* { */
/*     if(Moca_GetData(current)) */
/*     { */
/*         Moca_FixAllFalsePf(vma->vm_mm); */
/*         MOCA_DEBUG_PRINT("Unamp page handler task %p, mm %p\n", NULL, NULL); */
/*     } */
/*     jprobe_return(); */
/* } */


static struct jprobe Moca_PteFaultjprobe = {
    .entry = Moca_MmFaultHandler,
    .kp.symbol_name = "handle_mm_fault",
};

/* static struct jprobe Moca_UnmapPageProbe = { */
/*     .entry = Moca_UnmapPageHandler, */
/*     .kp.symbol_name = "unmap_page_range", */
/* }; */

int Moca_RegisterProbes(void)
{
    int ret;
    /* if ((ret=register_jprobe(&Moca_UnmapPageProbe))) */
    /*     Moca_Panic("Unable to register do exit probe"); */
    MOCA_DEBUG_PRINT("Moca registering probes\n");
    if ((ret=register_jprobe(&Moca_PteFaultjprobe)))
        Moca_Panic("Moca Unable to register pte fault probe");
    MOCA_DEBUG_PRINT("Moca registered probes\n");
    return ret;
}


void Moca_UnregisterProbes(void)
{
    /* unregister_jprobe(&Moca_UnmapPageProbe); */
    unregister_jprobe(&Moca_PteFaultjprobe);
}
