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
#include "memmap_pgtbl.h"

//Number of threads (one per CPU)
int MemMap_numThreads=1;
// Vector clock
double * MemMap_threadClocks=NULL;
// Thread task representation
struct task_struct **MemMap_threadTasks=NULL;

// Priority for FIFO scheduler
int MemMap_schedulerPriority=MEMMAP_DEFAULT_SCHED_PRIO;

int MemMap_NumThreads(void)
{
    return MemMap_numThreads;
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
    MemMap_numThreads=num_online_cpus();
    printk(KERN_WARNING "MemMap initializing %d threads\n",
            MemMap_numThreads);

    //Init threads data
    MemMap_threadClocks=kcalloc(MemMap_numThreads,sizeof(double),GFP_KERNEL);
    if(! MemMap_threadClocks)
    {
        MemMap_Panic("Vector clocks alloc failed");
        return -1;
    }

    MemMap_threadTasks=kcalloc(MemMap_numThreads, sizeof(void *),GFP_KERNEL);
    if(! MemMap_threadTasks)
    {
        MemMap_Panic("Thread tasks alloc failed");
        return -1;
    }

    // Create one monitor thread per CPU
    for(i=0;i< MemMap_numThreads;i++)
    {
        {
            printk(KERN_WARNING "Starting thread %d/%d\n", i, MemMap_numThreads);
            //Creating the thread
            MemMap_threadTasks[i]=kthread_create(MemMap_MonitorThread, NULL,
                    "MemMap tlb walker thread");
            printk("MemMap kthread %d create task %p\n", i, MemMap_threadTasks[i]);
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

    }
    return 0;
}

// Kill all remaining kthreads, and remove their memory
void MemMap_CleanThreads(void)
{
    int i;
    for(i=0;i<MemMap_numThreads;i++)
    {
        //Avoid suicidal call
        if(MemMap_threadTasks[i] && current != MemMap_threadTasks[i])
        {
            printk(KERN_WARNING "Killing thread %d/%d task %p\n", i,
                    MemMap_numThreads, MemMap_threadTasks[i]);
            kthread_stop(MemMap_threadTasks[i]);
            put_task_struct(MemMap_threadTasks[i]);
        }
    }
    printk(KERN_WARNING "All threads are dead\n");
    //Now we are safe: all threads are dead
    if(MemMap_threadTasks)
        kfree(MemMap_threadTasks);
    printk(KERN_WARNING "Thread tasks freed\n");
    if(MemMap_threadClocks)
        kfree(MemMap_threadClocks);
    printk(KERN_WARNING "Thread clocks freed\n");
}

