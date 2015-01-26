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
#define MOCA_DEBUG

#include <linux/kprobes.h>
#include "moca.h"
#include "moca_tasks.h"
#include "moca_taskdata.h"
#include "moca_probes.h"
#include "moca_page.h"
#include "moca_false_pf.h"

void Moca_FixPte(pte_t *pte)
{
    if (pte && MOCA_FALSE_PF(*pte))
    {
        MOCA_CLEAR_FALSE_PF(*pte);
        MOCA_DEBUG_PRINT("Moca fixing pte %p flags %x page %lx\n",pte,
                (unsigned int)pte_flags(*pte), (*(unsigned long*)pte)&PTE_PFN_MASK);
    }
}

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
    pte=Moca_PteFromAdress(address,mm);
    // Track only user pages
    if(!MOCA_USEFULL_PTE(pte))
        jprobe_return();
    MOCA_DEBUG_PRINT("Moca Pte fault task %p\n", current);
    MOCA_PRINT_FLAGS(pte);
    Moca_AddToChunk(data,(void *)(address&PAGE_MASK),get_cpu());
    Moca_FixPte(pte);
    Moca_UpdateClock();
    jprobe_return();

}

static int Moca_ExitHandler(struct kprobe *p, struct pt_regs *regs)
{
    int i,j,k,l;
    pgd_t *pgd;
    pmd_t *pmd;
    pud_t *pud;
    pte_t *pte;
    if(!Moca_GetData(current))
        return 0;
    MOCA_DEBUG_PRINT("Moca in do exit task %p\n", current);
    pgd=current->mm->pgd;//pgd_offset(current->mm,0);

    for (i=0;i<PTRS_PER_PGD;++i)
        if(!pgd_none(pgd[i]) && pgd_present(pgd[i]))
        {
            pud=pud_offset(pgd+i,0);
            for (j=0;j<PTRS_PER_PUD;++j)
                pmd=pmd_offset(pud +i, 0);
            for (k=0;k<PTRS_PER_PMD;++k)
                if(!pmd_none(pmd[k]) && pmd_present(pmd[k]))
                {
                    pte=(pte_t *)(pmd+k);//pte_offset(pmd+i,0);
                    for(l=0;l<PTRS_PER_PTE;++l)
                        if(MOCA_USEFULL_PTE(pte) && MOCA_FALSE_PF(*pte))
                            MOCA_CLEAR_FALSE_PF(*pte);
                }
        }
    return 0;
}


static struct jprobe Moca_PteFaultjprobe = {
    .entry = Moca_MmFaultHandler,
    .kp.symbol_name = "handle_mm_fault",
};

static struct kprobe Moca_Exitprobe = {
    .symbol_name = "do_exit",
};

int Moca_RegisterProbes(void)
{
    int ret;
    Moca_Exitprobe.pre_handler = Moca_ExitHandler;
    if ((ret=register_kprobe(&Moca_Exitprobe)))
        Moca_Panic("Unable to register do exit probe");
    if ((ret=register_jprobe(&Moca_PteFaultjprobe)))
        Moca_Panic("Moca Unable to register pte fault probe");
    return ret;
}


void Moca_UnregisterProbes(void)
{
    unregister_jprobe(&Moca_PteFaultjprobe);
    unregister_kprobe(&Moca_Exitprobe);
}
