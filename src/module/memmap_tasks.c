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
#include "memmap_tasks.h"


// The first bits are not random enough, 14 bits should be enough for pids
unsigned long MemMap_tasksHashBits=14UL;
int MemMap_tasksTableFactor=2;

// Monitored process
hash_map MemMap_tasksMap;
struct task_struct *MemMap_initTask=NULL;
spinlock_t MemMap_tasksLock;

int MemMap_AddTask(struct task_struct *t);

int MemMap_InitProcessManagment(int id)
{
    // Monitored pids
    struct pid *pid;
    spin_lock_init(&MemMap_tasksLock);
    MemMap_tasksMap=MemMap_InitHashMap(MemMap_tasksHashBits,
            MemMap_tasksTableFactor, sizeof(struct _memmap_task));
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
    MemMap_InitTaskData();
    return 0;

}

void MemMap_CleanProcessData(void)
{
    if(MemMap_tasksMap)
    {
        MemMap_ClearAllData();
        MemMap_FreeMap(MemMap_tasksMap);
    }
}

// Add pid to the monitored process if pid is a monitored process
int MemMap_AddTaskIfNeeded(unsigned long int id)
{
    struct pid *pid;
    int pos;
    struct task_struct *task, *ptask, *tmptask=NULL;
    // Get iternal pid representation
    rcu_read_lock();
    pid=find_vpid(id);

    if(!pid)
    {
        // Skip internal process
        rcu_read_unlock();
        MEMMAP_DEBUG_PRINT("MemMap process skiped %lu NULL\n", id);
        return 0;
    }
    task=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();
    if(!task)
        MemMap_Panic("Unable to add find task from pid\n");


    ptask=task->real_parent;
    //The task is a direct child of the init task
    if(ptask == MemMap_initTask)
        return MemMap_AddTask(task);
    spin_lock(&MemMap_tasksLock);
    if((pos=MemMap_PosInMap(MemMap_tasksMap ,ptask))!=-1)
        tmptask=(struct task_struct *)((memmap_task)(MemMap_EntryAtPos(MemMap_tasksMap,pos))->key);
    //Check if the parent is known
    spin_unlock(&MemMap_tasksLock);
    if (tmptask==ptask)
        return MemMap_AddTask(task);
    return 0;
}

// Current number of monitored pids
int MemMap_GetNumTasks(void)
{
    int nb=0;
    spin_lock(&MemMap_tasksLock);
    nb=MemMap_NbElementInMap(MemMap_tasksMap);
    spin_unlock(&MemMap_tasksLock);
    return nb;
}

task_data MemMap_GetData(struct task_struct *t)
{
    int pos;
    task_data ret=NULL;
    spin_lock(&MemMap_tasksLock);
    if((pos=MemMap_PosInMap(MemMap_tasksMap ,t))!=-1)
        ret=((memmap_task)MemMap_EntryAtPos(MemMap_tasksMap,pos))->data;
    spin_unlock(&MemMap_tasksLock);
    return ret;
}


// Add t to the monitored pids
int MemMap_AddTask(struct task_struct *t)
{
    task_data data;
    memmap_task tsk;
    int status;
    MEMMAP_DEBUG_PRINT("MemMap Adding task %p\n",t );


    //Create the task data
    data=MemMap_InitData(t);
    if(!data)
        return -1;
    spin_lock(&MemMap_tasksLock);

    tsk=(memmap_task)MemMap_AddToMap(MemMap_tasksMap,t,&status);
    switch(status)
    {
        case MEMMAP_HASHMAP_FULL:
            spin_unlock(&MemMap_tasksLock);
            MemMap_ClearData(data);
            MemMap_Panic("Too many pids");
            break;
        case MEMMAP_HASHMAP_ERROR:
            spin_unlock(&MemMap_tasksLock);
            MemMap_ClearData(data);
            MemMap_Panic("MemMap unhandeled hashmap error");
            break;
        case  MEMMAP_HASHMAP_ALREADY_IN_MAP:
            spin_unlock(&MemMap_tasksLock);
            MemMap_ClearData(data);
            MemMap_Panic("MemMap Adding an alreadt exixsting task");
            break;
        default:
            //normal add
            tsk->data=data;
            spin_unlock(&MemMap_tasksLock);
            MEMMAP_DEBUG_PRINT("MemMap Added task %p\n",t);
            break;
    }
    return 0;
}
