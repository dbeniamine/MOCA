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
#include "memmap_tlb.h"
#include "memmap_tasks.h"
#include "memmap_threads.h"
#include <linux/kthread.h>
#include <linux/smp.h> //get_cpu()
#include <linux/delay.h> //msleep
#include <linux/pid.h>

// Wakeup period in ms
int MemMap_wakeupInterval=MEMMAP_DEFAULT_WAKEUP_INTERVAL;

// Print out all recorded data
void flush_data(int id)
{
    //TODO
    printk(KERN_WARNING "Kthread %d flushing data \n", id);
}
//Walk through TLB and record all memory access
void MemMap_TLBWalk(int myId, struct task_struct *task)
{
    printk(KERN_WARNING "Kthread %d walking on task %p \n", myId, task);
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
int MemMap_MonitorTLBThread(void * arg)
{
    //Init tlb walk data
    int myId=get_cpu(),i;
    //Main loop
    while(!kthread_should_stop())
    {
        int nbTasks=MemMap_GetNumTasks();
        //For each process
        for(i=0;i<nbTasks;i++)
        {
            printk(KERN_WARNING "MemMap Kthread %d iterating task %d/%d\n",
                    myId, i,nbTasks);
            // For each threads
            if(MemMap_tasks[i]->on_cpu==myId)
            {
                // Do a TLB walk for each task on our CPU
                MemMap_TLBWalk(myId, MemMap_tasks[i]);
            }
        }
        if(MemMap_NeedToFlush(myId))
            flush_data(myId);
        msleep(MemMap_wakeupInterval);
        printk(KERN_WARNING "MemMap Kthread %d going to sleep for %d\n",
                myId, MemMap_wakeupInterval);
    }
    flush_data(myId);
    printk("MemMap thread %d finished\n", myId);
    return 0;
}

