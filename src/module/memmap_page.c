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

// Walk through the current chunk
void MemMap_MonitorPage(int myId,task_data data)
{
    int i=0,countR, countW;
    pte_t *pte;
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d walking on data %p , task %p\n",
            myId, data, MemMap_GetTaskFromData(data));
    MemMap_LockData(data);
    while((pte=(pte_t *)MemMap_AddrInChunkPos(data,i,MemMap_CurrentChunk(data)))!=NULL)
    {
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pagewalk pte %p ind %d cpu %d data %p\n",
                pte, i, myId, data);
        if(!pte_none(*pte) && pte_present(*pte) && !pte_special(*pte) )
        {
            *pte = pte_clear_flags(*pte, _PAGE_PRESENT);
            MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap FLAGS CLEARED pte %p\n", pte);
        }
        // Set R/W status
        //TODO: count perfctr
        if(pte_young(*pte))
            countR=1;
        if(pte_dirty(*pte))
            countW=1;
        MemMap_UpdateData(data,i,countR,countW, MemMap_CurrentChunk(data),myId);
        ++i;
    }
    // Goto to next chunk
    MemMap_NextChunks(data);
    MemMap_unLockData(data);
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap pagewalk pte cpu %d data %p end\n",
            myId,data);
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

    while(!kthread_should_stop())
    {
        int nbTasks=MemMap_GetNumTasks();
        //Freed in memmap_taskdata.h during flush or clear
        for(i=0;i<nbTasks;i++)
        {
            MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Kthread %d iterating taskdata %d/%d\n",
                    myId, i,nbTasks);
            data=((memmap_task)MemMap_EntryAtPos(MemMap_tasksMap,i))->data;
            task=MemMap_GetTaskFromData(data);
            MEMMAP_DEBUG_PRINT(KERN_WARNING "Kthread %d testing task %p\n", myId, task);
            if(!pid_alive(task))
            {
                MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap task %d %p is dead\n", i, task);
                //TODO manage task end
            }
            else if(task && task->on_cpu==myId && task->sched_info.last_arrival > lastwake)
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

