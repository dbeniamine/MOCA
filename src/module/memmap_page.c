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

// Walk through the current chunk
void MemMap_MonitorPage(int myId,task_data data, unsigned long long *clocks)
{
    int i=0;
    int type=MEMMAP_ACCESS_NONE, count;
    pte_t *pte;
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d walking on data %p , task %p\n",
            myId, data, MemMap_GetTaskFromData(data));
    MemMap_LockData(data);
    while((pte=(pte_t *)MemMap_AddrInChunkPos(data,i,MemMap_CurrentChunk(data)))!=NULL)
    {
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pagewalk pte %p ind %d\n", pte, i);
        *pte = pte_clear_flags(*pte, _PAGE_PRESENT);
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap FLAGS CLEARED pte %p\n", pte);
        // Set R/W status
        if(pte_young(*pte))
            type|=MEMMAP_ACCESS_R;
        if(pte_dirty(*pte))
            type|=MEMMAP_ACCESS_W;
        //TODO: count perfctr
        count=1;
        MemMap_UpdateData(data,i,type,count, MemMap_CurrentChunk(data));
        ++i;
    }
    // Goto to next chunk
    MemMap_NextChunks(data,clocks);
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
            MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d iterating taskdata %d/%d\n",
                    myId, i,nbTasks);
            data=MemMap_tasksData[i];
            task=MemMap_GetTaskFromData(data);
            MEMMAP_DEBUG_PRINT(KERN_WARNING "Kthread %d testing task %p\n", myId, task);
            if(task && task->on_cpu==myId && task->sched_info.last_arrival > lastwake)
            {
                lastwake=MAX(lastwake,task->sched_info.last_arrival);
                MEMMAP_DEBUG_PRINT(KERN_WARNING "KThread %d found task %p running on cpu %d\n",
                        myId, task, task->on_cpu);
                MemMap_MonitorPage(myId,data,clocks);
            }
        }
        MemMap_UpdateClock(myId);
        msleep(MemMap_wakeupInterval);
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d going to sleep for %d\n",
                myId, MemMap_wakeupInterval);
    }
    MEMMAP_DEBUG_PRINT("MemMap thread %d finished\n", myId);
    return 0;
}

