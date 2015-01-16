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
//#define MEMMAP_DEBUG

#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include "memmap.h"
#include "memmap_tasks.h"
#include <linux/spinlock.h>


// The first bits are not random enough, 14 bits should be enough for pids
unsigned long MemMap_tasksHashBits=14;
int MemMap_AddTaskWaiting=0;
spinlock_t MemMap_tasksLock;

// Monitored process
hash_map MemMap_tasksMap;
struct task_struct *MemMap_initTask=NULL;

memmap_task MemMap_AddTask(struct task_struct *t);

int MemMap_InitProcessManagment(int id)
{
    // Monitored pids
    struct pid *pid;
    spin_lock_init(&MemMap_tasksLock);
    MemMap_tasksMap=MemMap_InitHashMap(MemMap_tasksHashBits,
            2*(1<<MemMap_tasksHashBits), sizeof(struct _memmap_task));
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
memmap_task MemMap_AddTaskIfNeeded(struct task_struct *t)
{
    memmap_task tsk=NULL;
    spin_lock(&MemMap_tasksLock);
    if(t->real_parent == MemMap_initTask  ||
            MemMap_EntryFromKey(MemMap_tasksMap, t->real_parent)!=NULL )
        tsk=MemMap_AddTask(t);
    spin_unlock(&MemMap_tasksLock);
    return tsk;
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

memmap_task MemMap_NextTask(int *pos)
{
    memmap_task ret=NULL;
    MEMMAP_DEBUG_PRINT("MemMap Looking for task at %d\n", *pos);
    spin_lock(&MemMap_tasksLock);
    ret=(memmap_task)MemMap_NextEntryPos(MemMap_tasksMap,pos);
    MEMMAP_DEBUG_PRINT("MemMap found task %p at %d\n", ret, *pos);
    spin_unlock(&MemMap_tasksLock);
    return ret;
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
memmap_task MemMap_AddTask(struct task_struct *t)
{
    task_data data;
    memmap_task tsk;
    int status;
    MEMMAP_DEBUG_PRINT("MemMap Adding task %p\n",t );


    //Create the task data
    data=MemMap_InitData(t);
    get_task_struct(t);
    if(!data)
        return NULL;

    tsk=(memmap_task)MemMap_AddToMap(MemMap_tasksMap,t,&status);
    switch(status)
    {
        case MEMMAP_HASHMAP_FULL:
            MemMap_Panic("Too many pids");
            break;
        case MEMMAP_HASHMAP_ERROR:
            MemMap_Panic("MemMap unhandeled hashmap error");
            break;
        case  MEMMAP_HASHMAP_ALREADY_IN_MAP:
            MemMap_Panic("MemMap Adding an already exixsting task");
            break;
        default:
            //normal add
            tsk->data=data;
            MEMMAP_DEBUG_PRINT("MemMap Added task %p at pos %d \n", t, status);
            break;
    }
    return tsk;
}

void MemMap_RemoveTask(struct task_struct *t)
{
    spin_lock(&MemMap_tasksLock);
    MemMap_RemoveFromMap(MemMap_tasksMap, t);
    spin_unlock(&MemMap_tasksLock);
    put_task_struct(t);
}
