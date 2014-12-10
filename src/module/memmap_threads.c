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
#include <linux/kthread.h>
#include <linux/cpumask.h>  // num_online_cpus
#include <linux/slab.h>    // kcalloc /kfree
#include "memmap.h"
#include "memmap_threads.h"
#include "memmap_page.h"

// Vector clock
unsigned long long * MemMap_threadClocks=NULL;
// Thread task representation
struct task_struct **MemMap_threadTasks=NULL;

// Priority for FIFO scheduler
int MemMap_schedulerPriority=MEMMAP_DEFAULT_SCHED_PRIO;

// There are no locks for the clocks as they cannot be written in parrallel
// (only the ktrhead id is allowed to write the clock id). Moreover we do not
// care about read/write conflicts because in the worst case we loose a bit of
// precision which doesn't matters, and using locks would be slow.
void MemMap_UpdateClock(int id)
{
    MEMMAP_DEBUG_PRINT("MemMap updating clocks[%d]\n", id);
    ++MemMap_threadClocks[id];
}
void MemMap_GetClocks(unsigned long long *dst)
{
    int i;
    for(i=0;i<MemMap_NumThreads();i++)
    {
        dst[i]=MemMap_threadClocks[i];
    }
}

static void MemMap_SetSchedulerPriority(struct task_struct *task)
{
    struct sched_param param;
    param.sched_priority=MemMap_schedulerPriority;
    sched_setscheduler(task,SCHED_FIFO,&param);
}
// Initializes threads data structures
int MemMap_InitThreads(void)
{
    int i;
    MEMMAP_DEBUG_PRINT("MemMap initializing %d threads\n",
            MemMap_NumThreads());

    //Init threads data
    MemMap_threadClocks=kcalloc(MemMap_NumThreads(),sizeof(unsigned long long),GFP_KERNEL);
    if(! MemMap_threadClocks)
    {
        MemMap_Panic("MemMap Vector clocks alloc failed");
        return -1;
    }


    MemMap_threadTasks=kcalloc(MemMap_NumThreads(), sizeof(void *),GFP_KERNEL);
    if(! MemMap_threadTasks)
    {
        MemMap_Panic("Thread tasks alloc failed");
        return -1;
    }

    // Create one monitor thread per CPU
    for(i=0;i< MemMap_NumThreads();i++)
    {
        MEMMAP_DEBUG_PRINT("Starting thread %d/%d\n", i, MemMap_NumThreads());
        //Creating the thread
        MemMap_threadTasks[i]=kthread_create(MemMap_MonitorThread, NULL,
                "MemMap tlb walker thread");
        MEMMAP_DEBUG_PRINT("MemMap kthread %d create task %p\n", i, MemMap_threadTasks[i]);
        if(!MemMap_threadTasks[i])
        {
            MemMap_Panic("Kthread create failed");
            return -1;
        }
        get_task_struct(MemMap_threadTasks[i]);
        //Bind it on the ith proc
        kthread_bind(MemMap_threadTasks[i],i);
        // Set priority
        MemMap_SetSchedulerPriority(MemMap_threadTasks[i]);
        //And finally start it
        wake_up_process(MemMap_threadTasks[i]);

    }
    return 0;
}

void MemMap_StopThreads(void)
{
    int i;
    for(i=0;i<MemMap_NumThreads();i++)
    {
        //Avoid suicidal call
        if(MemMap_threadTasks[i] && current != MemMap_threadTasks[i])
        {
            MEMMAP_DEBUG_PRINT("Killing thread %d/%d task %p\n", i,
                    MemMap_NumThreads(), MemMap_threadTasks[i]);
            kthread_stop(MemMap_threadTasks[i]);
            put_task_struct(MemMap_threadTasks[i]);
        }
    }
    MEMMAP_DEBUG_PRINT("All threads are dead\n");
}
void MemMap_CleanThreads(void)
{
    //StopThreads should have been called before
    if(MemMap_threadTasks)
        kfree(MemMap_threadTasks);
    MEMMAP_DEBUG_PRINT("Thread tasks freed\n");
    if(MemMap_threadClocks)
        kfree(MemMap_threadClocks);
    MEMMAP_DEBUG_PRINT("Thread clocks freed\n");
}

