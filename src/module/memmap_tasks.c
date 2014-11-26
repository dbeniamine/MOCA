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
#include <linux/spinlock.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include "memmap.h"
#include "memmap_threads.h"
#include "memmap_taskdata.h"

#define MEMMAP_DEFAULT_MAX_PROCESS 32

// The first bits are not random enough, 14 bits should be enough for pids
#define MEMMAP_TASK_HASH_BITS 14UL
#define MEMMAP_HASH_SIZE (1UL<<MEMMAP_TASK_HASH_BITS)

// Monitored process
task_data *MemMap_tasksData;
int MemMap_tasksMap[MEMMAP_HASH_SIZE];
struct task_struct *MemMap_initTask=NULL;

// Current number of monitored pids
int MemMap_numTasks=0;
// Maximum Number of monitored pids (aka current alloc size on MemMap_tasks
int MemMap_maxTasks=MEMMAP_DEFAULT_MAX_PROCESS;

spinlock_t MemMap_tasksLock;

int MemMap_AddTask(struct task_struct *t);

int MemMap_InitProcessManagment(int maxprocs, int id)
{
    // Monitored pids
    int max,i;
    struct pid *pid;
    spin_lock_init(&MemMap_tasksLock);
    // Max allowed procs
    if(maxprocs > MEMMAP_DEFAULT_MAX_PROCESS)
    {
        max=maxprocs;
    }
    else
    {
        max=MEMMAP_DEFAULT_MAX_PROCESS;
    }
    spin_lock(&MemMap_tasksLock);
    MemMap_maxTasks=max;
    spin_unlock(&MemMap_tasksLock);


    MemMap_tasksData=kcalloc(max,sizeof(void *), GFP_KERNEL);
    if( !MemMap_tasksData)
    {
        MemMap_Panic("Tasks Alloc failed");
        return -1;
    }

    // Clear the hasmap
    for(i=0;i< MEMMAP_HASH_SIZE;++i)
        MemMap_tasksMap[i]=-1;

    rcu_read_lock();
    pid=find_vpid(id);

    if(!pid)
    {
        // Skip internal process
        rcu_read_unlock();
        MemMap_Panic("MemMap unable to find pid for init task");
        return 1;
    }
    MemMap_initTask=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();

    return 0;

}

void MemMap_CleanProcessData(void)
{
    int i, nbTasks;
    //Tell the kernel we don't need the tasks anymore
    if(MemMap_tasksData)
    {
        printk(KERN_WARNING "MemMap Cleaning data\n");
        nbTasks=MemMap_GetNumTasks();
        for(i=0;i<nbTasks;++i)
        {
            MemMap_ClearData(MemMap_tasksData[i]);
        }
        printk(KERN_WARNING "MemMap Cleaning all data\n");
        kfree(MemMap_tasksData);
    }
}

// Add pid to the monitored process if pid is a monitored process
int MemMap_AddTaskIfNeeded(int id)
{
    struct pid *pid;
    struct task_struct *task, *ptask, *tmptask=NULL;
    u32 h;
    // Get iternal pid representation
    rcu_read_lock();
    pid=find_vpid(id);

    if(!pid)
    {
        // Skip internal process
        rcu_read_unlock();
        printk(KERN_WARNING "MemMap process skiped %d NULL\n", id);
        return 0;
    }
    task=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();


    ptask=task->real_parent;
    //The task is a direct child of the init task
    if(ptask == MemMap_initTask)
        return MemMap_AddTask(task);
    //Check if the parent is known
    h=hash_ptr(ptask, MEMMAP_TASK_HASH_BITS);
    spin_lock(&MemMap_tasksLock);
    if(MemMap_tasksMap[h]!=-1)
        tmptask=MemMap_GetTaskFromData(MemMap_tasksData[MemMap_tasksMap[h]]);
    spin_unlock(&MemMap_tasksLock);
    if (tmptask==ptask)
        return MemMap_AddTask(task);
    return 0;
}

// Current number of monitored pids
int MemMap_GetNumTasks(void)
{
    int nb;
    spin_lock(&MemMap_tasksLock);
    nb=MemMap_numTasks;
    spin_unlock(&MemMap_tasksLock);
    return nb;
}

task_data MemMap_GetData(struct task_struct *t)
{
    task_data ret=NULL;
    u32 h=hash_ptr(t,MEMMAP_TASK_HASH_BITS);
    spin_lock(&MemMap_tasksLock);
    if(MemMap_tasksMap[h]!=-1)
        ret=MemMap_tasksData[MemMap_tasksMap[h]];
    spin_unlock(&MemMap_tasksLock);
    return ret;
}


// Add t to the monitored pids
int MemMap_AddTask(struct task_struct *t)
{
    task_data data;
    u32 h;
    printk(KERN_WARNING "MemMap Adding task %p\n",t );

    h= hash_ptr(t,MEMMAP_TASK_HASH_BITS);

    //Create the task data
    data=MemMap_InitData(t);
    if(!data)
        return -1;
    //Get number and max of pids
    spin_lock(&MemMap_tasksLock);
    if(MemMap_numTasks>=MemMap_maxTasks)
    {
        spin_unlock(&MemMap_tasksLock);
        MemMap_ClearData(data);
        MemMap_Panic("Too many pids");
        return -1;
    }
    //Check if the task must be added
    if(MemMap_tasksMap[h]!=-1)
    {
        spin_unlock(&MemMap_tasksLock);
        MemMap_ClearData(data);
        MemMap_Panic("MemMap Hash conflict");
        return -1;
    }
    MemMap_tasksMap[h]=MemMap_numTasks;
    MemMap_tasksData[MemMap_numTasks]=data;
    ++MemMap_numTasks;
    spin_unlock(&MemMap_tasksLock);

    printk(KERN_WARNING "MemMap Added task %p\n",t);
    return 0;
}


