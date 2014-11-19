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
#include "memmap_pgtbl.h"
#include "memmap_tasks.h"
#include "memmap_threads.h"
#include <linux/kthread.h>
#include <linux/smp.h> //get_cpu()
#include <linux/delay.h> //msleep
#include <linux/pid.h>
#include <asm/pgtable.h>

// Wakeup period in ms
int MemMap_wakeupInterval=MEMMAP_DEFAULT_WAKEUP_INTERVAL;

// Print out all recorded data
void flush_data(int id)
{
    //TODO
    printk(KERN_WARNING "Kthread %d flushing data \n", id);
}
void MemMap_DoPageWalk(int id, struct mm_struct *mm)
{
    pmd_t *pmd;
    pte_t *pte;
    pgd_t *pgd=mm->pgd;//pgd_offset(mm,0);
    //struct page *p;
    int i,j,k, nb_present=0;

    if(!pgd_present(*pgd))
        return;
    for (i=0;i<PTRS_PER_PGD;++i)
        if(!pgd_none(pgd[i]) && pgd_present(pgd[i]))
        {
            pmd=(pmd_t *)(pgd+1);//pmd_offset(pgd +i, 0);
            for (j=0;j<PTRS_PER_PMD;++j)
                if(!pmd_none(pmd[j]) && pmd_present(pmd[j]))
                {
                    pte=(pte_t *)(pmd+j);//pte_offset(pmd+i,0);
                    for(k=0;k<PTRS_PER_PTE;++k)
                        if(!pte_none(pte[k]) &&pte_present(pte[k]))
                        {
                            ++nb_present;
                        }
                }

        }
    printk(KERN_WARNING "Kthread %d found %d pages during walk\n", id, nb_present);

}

//Walk through TLB and record all memory access
void MemMap_pgtb_Walk(int myId, struct task_struct *task)
{
    printk(KERN_WARNING "Kthread %d walking on task %p with sched prio %d inverted %d\n",
            myId, task, task->prio, MEMMAP_DEFAULT_SCHED_PRIO-task->prio);
    MemMap_DoPageWalk(myId, task->mm);
}
// Return 1 iff too many data are recorded
int MemMap_NeedToFlush(int id)
{
    //TODO
    return 0;
}

/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int MemMap_MonitorThread(void * arg)
{
    int cpt=0;
    //Init tlb walk data
    int myId=get_cpu(),i;
    unsigned long long lastwake=0;
    while(!kthread_should_stop())
    {
        int nbTasks=MemMap_GetNumTasks(myId);

        for(i=0;i<nbTasks;i++)
        {
            printk(KERN_WARNING "MemMap Kthread %d iterating task %d/%d\n",
                    myId, i,nbTasks);
            //TODO: Walk only on task which were modified since last wake
            if(MemMap_tasks[myId][i]->sched_info.last_arrival > lastwake )
            {
                lastwake=MAX(lastwake,MemMap_tasks[myId][i]->sched_info.last_arrival);
                printk(KERN_WARNING "KThread %d found task %p running on cpu %d\n",
                        myId, MemMap_tasks[myId][i], MemMap_tasks[myId][i]->on_cpu);
                MemMap_pgtb_Walk(myId, MemMap_tasks[myId][i]);
            }
        }
        if(MemMap_NeedToFlush(myId))
            flush_data(myId);
        msleep(MemMap_wakeupInterval);
        printk(KERN_WARNING "MemMap Kthread %d going to sleep for %d\n",
                myId, MemMap_wakeupInterval);
        ++cpt;
        if(cpt>=10)
            break;
    }
    flush_data(myId);
    printk("MemMap thread %d finished\n", myId);
    MemMap_Panic("End of thread\n");
    return 0;
}

