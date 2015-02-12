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
    Moca_AddToChunk(data,(void *)(address&PAGE_MASK),get_cpu());
    MOCA_DEBUG_PRINT("Moca Pte fault task %p\n", current);
    pte=Moca_PteFromAdress(address&PAGE_MASK,mm);
    if(Moca_FixFalsePf(mm,pte)!=0)
        MOCA_DEBUG_PRINT("Moca true pte fault at %p %p \n", pte, mm);
    Moca_UpdateClock();
    jprobe_return();

}

int Moca_ExitHandler(struct kprobe* k, struct pt_regs* r)
{
    if(!Moca_GetData(current))
        return 0;
    MOCA_DEBUG_PRINT("Exit handler handler task %p, mm %p\n", NULL, NULL);
    Moca_FixAllFalsePf(current->mm);
    return 0;
}


static struct jprobe Moca_PteFaultjprobe = {
    .entry = Moca_MmFaultHandler,
    .kp.symbol_name = "handle_mm_fault",
};

static struct kprobe Moca_ExitProbe = {
    .symbol_name = "do_exit",
};

int Moca_RegisterProbes(void)
{
    int ret;
    Moca_ExitProbe.pre_handler=Moca_ExitHandler;
    if ((ret=register_kprobe(&Moca_ExitProbe)))
        Moca_Panic("Unable to register do exit probe");
    MOCA_DEBUG_PRINT("Moca registering probes\n");
    if ((ret=register_jprobe(&Moca_PteFaultjprobe)))
        Moca_Panic("Moca Unable to register pte fault probe");
    MOCA_DEBUG_PRINT("Moca registered probes\n");
    return ret;
}


void Moca_UnregisterProbes(void)
{
    unregister_kprobe(&Moca_ExitProbe);
    unregister_jprobe(&Moca_PteFaultjprobe);
}
