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
#define MEMMAP_DEBUG

#include "memmap.h"
#include "memmap_page.h"
#include "memmap_taskdata.h"
#include "memmap_tasks.h"
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
    if (!pgd || pgd_none(*pgd) || pgd_bad(*pgd))
        return NULL;
    pud = pud_offset(pgd, address);
    if(!pud || pud_none(*pud) || pud_bad(*pud))
        return NULL;
    pmd = pmd_offset(pud, address);
    if (!pmd || pmd_none(*pmd) || pmd_bad(*pmd))
        return NULL;
    return pte_offset_map(pmd, address);

}

// Walk through the current chunk
void MemMap_MonitorPage(task_data data)
{
    int i=0,countR, countW;
    struct task_struct *tsk=MemMap_GetTaskFromData(data);
    pte_t *pte;
    void *addr;
    MEMMAP_DEBUG_PRINT("MemMap monitor thread walking data %p , task %p, mm %p\n",
            data, tsk, tsk->mm);
    while((addr=MemMap_AddrInChunkPos(data,i))!=NULL)
    {
        pte=MemMap_PteFromAdress((unsigned long)addr,MemMap_GetTaskFromData(data)->mm);
        MEMMAP_DEBUG_PRINT("MemMap pagewalk addr : %p pte %p ind %d cpu %d data %p\n",
                addr, pte, i,tsk->on_cpu, data);
        if(pte)
        {
            if(!pte_none(*pte) && pte_present(*pte) && !pte_special(*pte) )
            {
                *pte = pte_clear_flags(*pte, _PAGE_PRESENT);
                MEMMAP_DEBUG_PRINT("MemMap FLAGS CLEARED pte %p\n", pte);
            }
            // Set R/W status
            //TODO: count perfctr
            if(pte_young(*pte))
                countR=1;
            if(pte_dirty(*pte))
                countW=1;
            MemMap_UpdateData(data,i,countR,countW,tsk->on_cpu);
        }
        else
            MEMMAP_DEBUG_PRINT("MemMap no pte for adress %p\n", addr);
        ++i;
    }
    // Goto to next chunk
    MemMap_NextChunks(data);
    MEMMAP_DEBUG_PRINT("MemMap pagewalk pte cpu %d data %p end\n",
            tsk->on_cpu,data);
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
    unsigned int i;
    unsigned long long lastwake=0;

    while(!kthread_should_stop())
    {
        i=0;
        while((t=MemMap_NextTask(&i)))
        {
            data=t->data;
            task=(struct task_struct *)t->key;
            MEMMAP_DEBUG_PRINT("MemMap monitor thread testing task %p\n", task);
            if(pid_alive(task) && task->sched_info.last_arrival > lastwake)
            {
                lastwake=MAX(lastwake,task->sched_info.last_arrival);
                MEMMAP_DEBUG_PRINT("MemMap monitor thread found task %p\n",task);
                MemMap_LockChunk(data);
                // Here we are sure that the monitored task does not held an
                // important lock therefore we can stop it
                kill_pid(task_pid(task), SIGSTOP, 1);
                MemMap_UnlockChunk(data);
                MemMap_MonitorPage(data);
                kill_pid(task_pid(task), SIGCONT, 1);
            }
        }
        MemMap_UpdateClock();
        MEMMAP_DEBUG_PRINT("MemMap monitor thread going to sleep for %d\n",
                MemMap_wakeupInterval);
        msleep(MemMap_wakeupInterval);
    }
    MEMMAP_DEBUG_PRINT("MemMap monitor thread finished\n");
    return 0;
}

