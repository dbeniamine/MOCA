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

#include "moca.h"
#include "moca_page.h"
#include "moca_taskdata.h"
#include "moca_tasks.h"
#include "moca_false_pf.h"
#include <linux/kthread.h>
#include <linux/smp.h> //get_cpu()
#include <linux/delay.h> //msleep
#include <linux/slab.h>
#include <linux/pid.h>
#include <asm/pgtable.h>

// Wakeup period in ms
int Moca_wakeupInterval=MOCA_DEFAULT_WAKEUP_INTERVAL;

void *Moca_PhyFromVirt(void *addr, struct mm_struct *mm)
{
    return addr; //TODO fixme
    /*
       pte_t *pte=Moca_PteFromAdress((unsigned long)addr,mm);
       if(!pte)
       return NULL;
       return (void *)__pa(page_address(pte_page(*pte)));
       */
}

pte_t *Moca_PteFromAdress(unsigned long address, struct mm_struct *mm)
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
void Moca_MonitorPage(task_data data)
{
    int i=0,countR, countW;
    struct task_struct *tsk=Moca_GetTaskFromData(data);
    pte_t *pte;
    void *addr;
    MOCA_DEBUG_PRINT("Moca monitor thread walking data %p , task %p, mm %p\n",
            data, tsk, tsk->mm);
    while((addr=Moca_AddrInChunkPos(data,i))!=NULL)
    {
        pte=Moca_PteFromAdress((unsigned long)addr,Moca_GetTaskFromData(data)->mm);
        MOCA_DEBUG_PRINT("Moca pagewalk addr : %p pte %p ind %d cpu %d data %p\n",
                addr, pte, i,tsk->on_cpu, data);
        if(pte)
        {
            //Moca_AddFalsePf(tsk->mm, pte);
            // Set R/W status
            //TODO: count perfctr
            if(pte_young(*pte))
                countR=1;
            if(pte_dirty(*pte))
                countW=1;
            Moca_UpdateData(data,i,countR,countW,tsk->on_cpu);
        }
        else
            MOCA_DEBUG_PRINT("Moca no pte for adress %p\n", addr);
        ++i;
    }
    // Goto to next chunk
    Moca_NextChunks(data);
    MOCA_DEBUG_PRINT("Moca pagewalk pte cpu %d data %p end\n",
            tsk->on_cpu,data);
}


/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int Moca_MonitorThread(void * arg)
{
    task_data data;
    moca_task t;
    struct task_struct * task;
    //Init tlb walk data
    unsigned int i;
    unsigned long long lastwake=0;

    while(!kthread_should_stop())
    {
        i=0;
        while((t=Moca_NextTask(&i)))
        {
            data=t->data;
            task=(struct task_struct *)t->key;
            MOCA_DEBUG_PRINT("Moca monitor thread testing task %p\n", task);
            if(pid_alive(task) && task->sched_info.last_arrival > lastwake)
            {
                lastwake=MAX(lastwake,task->sched_info.last_arrival);
                MOCA_DEBUG_PRINT("Moca monitor thread found task %p\n",task);
                Moca_LockChunk(data);
                // Here we are sure that the monitored task does not held an
                // important lock therefore we can stop it
                kill_pid(task_pid(task), SIGSTOP, 1);
                Moca_UnlockChunk(data);
                Moca_MonitorPage(data);
                kill_pid(task_pid(task), SIGCONT, 1);
            }
        }
        Moca_UpdateClock();
        MOCA_DEBUG_PRINT("Moca monitor thread going to sleep for %d\n",
                Moca_wakeupInterval);
        msleep(Moca_wakeupInterval);
    }
    MOCA_DEBUG_PRINT("Moca monitor thread finished\n");
    return 0;
}

