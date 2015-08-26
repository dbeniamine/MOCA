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
    int i=0,countR=0, countW=0;
    struct task_struct *tsk=Moca_GetTaskFromData(data);
    void *addr;
    MOCA_DEBUG_PRINT("Moca monitor thread walking data %p , task %p, mm %p\n",
            data, tsk, tsk->mm);
    while((addr=Moca_AddrInChunkPos(data,i))!=NULL)
    {
        MOCA_DEBUG_PRINT("Moca pagewalk addr : %p ind %d cpu %d data %p\n",
            addr, i,tsk->on_cpu, data);
            //TODO: count perfctr
        Moca_AddFalsePf(tsk->mm, (unsigned long)addr,&countR,&countW);
        Moca_UpdateData(data,i,countR,countW,tsk->on_cpu);
        ++i;
    }
    // Goto to next chunk
    Moca_NextChunks(data);
    MOCA_DEBUG_PRINT("Moca monitor page cpu %d data %p end\n",
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

    /* dump_stack(); */
    MOCA_DEBUG_PRINT("Moca monitor thread alive \n");
    while(!kthread_should_stop())
    {
        pos=0;
        /* dump_stack(); */
        while((t=Moca_NextTask(&pos)))
        {
            data=t->data;
            task=(struct task_struct *)(t->key);
            MOCA_DEBUG_PRINT("Moca monitor thread testing task %p\n", task);
            if(pid_alive(task) && task->sched_info.last_arrival >= lastwake)
            {
                lastwake=task->sched_info.last_arrival;
                MOCA_DEBUG_PRINT("Moca monitor thread found task %p\n",task);
                Moca_LockChunk(data);
                Moca_LockPf();
                // Here we are sure that the monitored task does not held an
                // important lock therefore we can stop it
                kill_pid(task_pid(task), SIGSTOP, 1);
                Moca_UnlockPf();
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

