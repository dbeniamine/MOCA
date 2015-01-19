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
//#define MOCA_DEBUG

#include <linux/kprobes.h>
#include "moca.h"
#include "moca_tasks.h"
#include "moca_taskdata.h"
#include "moca_probes.h"
#include "moca_page.h"

//static int Moca_DoExitHandler(struct kprobe *p, struct pt_regs *regs)
//{
//    int i,j,k,l;
//    pgd_t *pgd=pgd_offset(current->mm,0);
//    pmd_t *pmd;
//    pud_t *pud;
//    pte_t *pte;
//    MOCA_DEBUG_PRINT("Moca in do exit task %p\n", current);
//
//    for (i=0;i<PTRS_PER_PGD;++i)
//        if(!pgd_none(pgd[i]) && pgd_present(pgd[i]))
//        {
//            pud=pud_offset(pgd+i,0);
//            for (j=0;j<PTRS_PER_PUD;++j)
//                pmd=pmd_offset(pud +i, 0);
//            for (k=0;k<PTRS_PER_PMD;++k)
//                if(!pmd_none(pmd[k]) && pmd_present(pmd[k]))
//                {
//                    pte=(pte_t *)(pmd+k);//pte_offset(pmd+i,0);
//                    for(l=0;l<PTRS_PER_PTE;++l)
//                        if(!pte_none(pte[l]) && !pte_present(pte[l]) &&
//                                !pte_special(pte[l]))
//                        {
//                            *pte = pte_set_flags(*pte, _PAGE_PRESENT);
//                        }
//                }
//        }
//    return 0;
//}

void Moca_MmFaultHandler(struct mm_struct *mm, struct vm_area_struct *vma,
        unsigned long address, unsigned int flags)
{
    pte_t *pte;

    task_data data;
    moca_task tsk;
    //MOCA_DEBUG_PRINT("Moca Pte fault task %p\n", current);
    if(!(data=Moca_GetData(current)))
    {
        if(!(tsk=Moca_AddTaskIfNeeded(current)))
            jprobe_return();
        data=tsk->data;
    }
    Moca_AddToChunk(data,(void *)address,get_cpu());
    pte=Moca_PteFromAdress(address,mm);
    //If pte exists, try to fix false pagefault
    if (pte && !pte_none(*pte) && !pte_present(*pte) && !pte_special(*pte))
    {
        *pte = pte_set_flags(*pte, _PAGE_PRESENT);
        MOCA_DEBUG_PRINT("Moca fixing fake pagefault\n");
    }
    Moca_UpdateClock();
    jprobe_return();

}

//static struct kprobe Moca_doExitprobe = {
//    .symbol_name = "do_exit",
//};

static struct jprobe Moca_PteFaultjprobe = {
    .entry = Moca_MmFaultHandler,
    .kp.symbol_name = "handle_mm_fault",
};
int Moca_RegisterProbes(void)
{
    int ret;
//    Moca_doExitprobe.pre_handler = Moca_DoExitHandler;
//    if ((ret=register_kprobe(&Moca_doExitprobe)))
//        Moca_Panic("Unable to register do exit probe");
    if ((ret=register_jprobe(&Moca_PteFaultjprobe)))
        Moca_Panic("Moca Unable to register pte fault probe");
    return ret;
}


void Moca_UnregisterProbes(void)
{
    unregister_jprobe(&Moca_PteFaultjprobe);
//    unregister_kprobe(&Moca_doExitprobe);
}
