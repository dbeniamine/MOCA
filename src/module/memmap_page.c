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
#define __NO_VERSION__
#include "memmap.h"
#include "memmap_page.h"
#include "memmap_taskdata.h"
#include "memmap_threads.h"
#include <linux/kthread.h>
#include <linux/smp.h> //get_cpu()
#include <linux/delay.h> //msleep
#include <linux/slab.h>
#include <linux/pid.h>
#include <asm/pgtable.h>

// Wakeup period in ms
int MemMap_wakeupInterval=MEMMAP_DEFAULT_WAKEUP_INTERVAL;

/* pte_t *MemMap_PageWalk(void *addr, struct mm_struct *mm) */
/* { */
/*     long address=(long)addr; */
/*     pgd_t *pgd=pgd_offset(mm,address); */
/*     pmd_t *pmd; */
/*     pud_t *pud; */
/*     pte_t *pte; */
/*     if (!pgd_none(*pgd) && !pgd_bad(*pgd)) */
/*     { */
/*         pud=pud_offset(pgd,address); */
/*         if(!pud_none(*pud) && !pud_bad(*pud)) */
/*         { */
/*             pmd = pmd_offset(pud, address); */
/*             if (!pmd_none(*pmd) && !pmd_bad(*pmd)) */
/*             { */
/*                 pte = pte_offset_map(pmd, address); */
/*                 if (!pte_none(*pte) && pte_present(*pte)) */
/*                 { */
/*                     return pte; */
/*                 } */
/*             } */
/*         } */
/*     } */
/*     return NULL; */
/* } */

//Walk through TLB and record all memory access
void MemMap_MonitorPage(int myId,task_data data, unsigned long long *clocks)
{
    int i=0;
    /* int type; */
    pte_t *pte;
    struct task_struct *t=MemMap_GetTaskFromData(data);
    printk(KERN_WARNING "Kthread %d walking on task %p with sched prio %d inverted %d\n",
            myId, t, t->prio, MEMMAP_DEFAULT_SCHED_PRIO-t->prio);
    MemMap_LockData(data);
    while((pte=(pte_t *)MemMap_AddrInChunkPos(data,i,MemMap_CurrentChunk(data)))!=NULL)
    {
        /* pte=MemMap_PageWalk(addr, MemMap_GetTaskFromData(data)->mm); */
        printk(KERN_WARNING "MemMAp pagewalk pte %p ind %d\n", pte, i);
        //TODO perfctr for count
        *pte = pte_clear_flags(*pte, _PAGE_PRESENT);
        printk(KERN_WARNING "MemMAp FLAGS CLEARED pte %p\n", pte);
        // Set R/W status + nb access
        // TODO
        ++i;
    }
    // Goto to next chunk
    /* MemMap_NextChunks(data,clocks); */
    MemMap_unLockData(data);
}


/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int MemMap_MonitorThread(void * arg)
{
    task_data data;
    struct task_struct *task;
    //Init tlb walk data
    int myId=get_cpu(),i;
    unsigned long long lastwake=0;
    unsigned long long *clocks;

    while(!kthread_should_stop())
    {
        int nbTasks=MemMap_GetNumTasks();
        //Freed in memmap_taskdata.h during flush or clear
        clocks=kcalloc(MemMap_NumThreads(),sizeof(unsigned long long),GFP_ATOMIC);
        MemMap_GetClocks(clocks);
        for(i=0;i<nbTasks;i++)
        {
            printk(KERN_WARNING "MemMap Kthread %d iterating taskdata %d/%d\n",
                    myId, i,nbTasks);
            data=MemMap_tasksData[i];
            task=MemMap_GetTaskFromData(data);
            printk(KERN_WARNING "Kthread %d testing task %p\n", myId, task);
            if(task && task->on_cpu==myId && task->sched_info.last_arrival > lastwake)
            {
                lastwake=MAX(lastwake,task->sched_info.last_arrival);
                printk(KERN_WARNING "KThread %d found task %p running on cpu %d\n",
                        myId, task, task->on_cpu);
                MemMap_MonitorPage(myId,data,clocks);
            }
        }
        MemMap_UpdateClock(myId);
        msleep(MemMap_wakeupInterval);
        printk(KERN_WARNING "MemMap Kthread %d going to sleep for %d\n",
                myId, MemMap_wakeupInterval);
    }
    printk("MemMap thread %d finished\n", myId);
    return 0;
}

