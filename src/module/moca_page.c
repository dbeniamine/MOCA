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
/* #define MOCA_DEBUG */

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

// Wakeup period in ms
int Moca_wakeupInterval=MOCA_DEFAULT_WAKEUP_INTERVAL;



// Walk through the current chunk
void Moca_MonitorPage(task_data data)
{
    int i=0,ch;
    struct task_struct *tsk=Moca_GetTaskFromData(data);
    void *addr;
    MOCA_DEBUG_PRINT("Moca monitor thread walking data %p , task %p, mm %p\n",
            data, tsk, tsk->mm);
    // Goto to next chunk
    MOCA_DEBUG_PRINT("Moca monitor next chunks, data %p\n",data);
    ch=Moca_NextChunks(data);
    while((addr=Moca_AddrInChunkPos(data,&i,ch))!=NULL)
    {
        MOCA_DEBUG_PRINT("Moca monitor addr : %p ind %d cpu %d data %p\n",
                addr, i,tsk->on_cpu, data);
        Moca_WLockPf();
        Moca_AddFalsePf(tsk->mm, (unsigned long)addr);
        Moca_WUnlockPf();
    }
    Moca_EndChunk(data,ch);
    MOCA_DEBUG_PRINT("Moca monitor cpu %d data %p end\n",
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
    int pos;
    unsigned long long lastwake=0;

    MOCA_DEBUG_PRINT("Moca monitor thread alive \n");
    while(!kthread_should_stop())
    {
        pos=0;
        while((t=Moca_NextTask(&pos)))
        {
            data=t->data;
            task=(struct task_struct *)(t->key);
            MOCA_DEBUG_PRINT("Moca monitor thread testing task %p\n", task);
            if(pid_alive(task) && task->sched_info.last_arrival >= lastwake)
            {
                lastwake=task->sched_info.last_arrival;
                MOCA_DEBUG_PRINT("Moca monitor thread found task %p\n",task);
                Moca_MonitorPage(data);
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

