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
#include "memmap_tasks.h"
#include "memmap_threads.h"
#include <linux/kthread.h>
#include <linux/smp.h> //get_cpu()
#include <linux/delay.h> //msleep
#include <linux/slab.h>
#include <linux/pid.h>
#include <asm/pgtable.h>

// Wakeup period in ms
int MemMap_wakeupInterval=MEMMAP_DEFAULT_WAKEUP_INTERVAL;

pte_t *MemMap_PteFromAdress(unsigned long address, struct mm_struct *mm)
{
    pgd_t *pgd;
    pmd_t *pmd;
    pud_t *pud;
    pgd = pgd_offset(mm, address);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return NULL;
    pud = pud_offset(pgd, address);
    if(pud_none(*pud) || pud_bad(*pud))
        return NULL;
    pmd = pmd_offset(pud, address);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
        return NULL;
    return pte_offset_map(pmd, address);

}

// Walk through the current chunk
void MemMap_MonitorPage(int myId,task_data data)
{
    //TODO: fix pte
    int i=0,countR, countW;
    pte_t *pte;
    void *addr;
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d walking on data %p , task %p\n",
            myId, data, MemMap_GetTaskFromData(data));
    while((addr=MemMap_AddrInChunkPos(data,i))!=NULL)
    {
        pte=MemMap_PteFromAdress((unsigned long)addr,MemMap_GetTaskFromData(data)->mm);
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pagewalk addr : %p pte %p ind %d cpu %d data %p\n",
                addr, pte, i, myId, data);
        if(pte)
        {
            if(!pte_none(*pte) && pte_present(*pte) && !pte_special(*pte) )
            {
                /* *pte = pte_clear_flags(*pte, _PAGE_PRESENT); */
                MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap FLAGS CLEARED pte %p\n", pte);
            }
            // Set R/W status
            //TODO: count perfctr
            if(pte_young(*pte))
                countR=1;
            if(pte_dirty(*pte))
                countW=1;
            MemMap_UpdateData(data,i,countR,countW,myId);
        }
        else
            MEMMAP_DEBUG_PRINT("MemMap not pte for adress %p\n", addr);
        ++i;
    }
    // Goto to next chunk
    MemMap_NextChunks(data);
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pagewalk pte cpu %d data %p end\n",
            myId,data);
}


/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int MemMap_MonitorThread(void * arg)
{
    task_data data;
    memmap_task t;
    struct task_struct * task;
    //Init tlb walk data
    unsigned int myId=get_cpu(),i=0;
    unsigned long long lastwake=0;

    while(!kthread_should_stop())
    {
        while((t=((memmap_task)MemMap_NextEntryPos(MemMap_tasksMap,&i))))
        {
            data=t->data;
            task=(struct task_struct *)t->key;
            MEMMAP_DEBUG_PRINT(KERN_WARNING "Kthread %d testing task %p\n",
                    myId, task);
            if(!pid_alive(task))
            {
                MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap task %d %p is dead\n",
                        i, task);
                //TODO manage task end
            }
            else if(task && task->on_cpu==myId
                    && task->sched_info.last_arrival > lastwake)
            {
                lastwake=MAX(lastwake,task->sched_info.last_arrival);
                MEMMAP_DEBUG_PRINT(KERN_WARNING "KThread %d found task %p running on cpu %d\n",
                        myId, task, task->on_cpu);
                MemMap_MonitorPage(myId,data);
            }
        }
        MemMap_UpdateClock(myId);
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d going to sleep for %d\n",
                myId, MemMap_wakeupInterval);
        msleep(MemMap_wakeupInterval);
    }
    MEMMAP_DEBUG_PRINT("MemMap thread %d finished\n", myId);
    return 0;
}

