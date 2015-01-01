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
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/hash.h>
#include <linux/slab.h>
#include "memmap.h"
#include "memmap_threads.h"
#include "memmap_tasks.h"
#include "memmap_lock.h"


// The first bits are not random enough, 14 bits should be enough for pids
unsigned long MemMap_tasksHashBits=14UL;
int MemMap_tasksTableFactor=2;
int MemMap_AddTaskWaiting=0;
MemMap_Lock_t MemMap_tasksLock;

// Monitored process
hash_map MemMap_tasksMap;
struct task_struct *MemMap_initTask=NULL;

int MemMap_AddTask(struct task_struct *t);

int MemMap_InitProcessManagment(int id)
{
    // Monitored pids
    struct pid *pid;
    MemMap_tasksLock=MemMap_Lock_Init();
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
    int pos, ret=0;
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
    MemMap_Lock(MemMap_tasksLock, MEMMAP_LOCK_PRIO_MAX);
    if(ptask == MemMap_initTask)
    {
        //The task is a direct child of the init task
        ret=MemMap_AddTask(task);
    }
    else
    {
        if((pos=MemMap_PosInMap(MemMap_tasksMap ,ptask))!=-1)
            tmptask=(struct task_struct *)((memmap_task)(MemMap_EntryAtPos(MemMap_tasksMap,pos))->key);
        //Check if the parent is known
        if (tmptask==ptask)
            ret=MemMap_AddTask(task);
    }
    MemMap_Unlock(MemMap_tasksLock);
    return ret;
}

// Current number of monitored pids
int MemMap_GetNumTasks(void)
{
    int nb=0;
    MemMap_Lock(MemMap_tasksLock, MEMMAP_LOCK_PRIO_MIN);
    nb=MemMap_NbElementInMap(MemMap_tasksMap);
    MemMap_Unlock(MemMap_tasksLock);
    return nb;
}

task_data MemMap_GetData(struct task_struct *t)
{
    int pos;
    task_data ret=NULL;
    if((pos=MemMap_PosInMap(MemMap_tasksMap ,t))!=-1)
        ret=((memmap_task)MemMap_EntryAtPos(MemMap_tasksMap,pos))->data;
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
    get_task_struct(t);
    if(!data)
        return -1;

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
            MemMap_Panic("MemMap Adding an alreadt exixsting task");
            break;
        default:
            //normal add
            tsk->data=data;
            MEMMAP_DEBUG_PRINT("MemMap Added task %p at pos %d \n", t, status);
            break;
    }
    return 0;
}

void MemMap_RemoveTask(struct task_struct *t)
{
    MemMap_Lock(MemMap_tasksLock, MEMMAP_LOCK_PRIO_MIN);
    MemMap_RemoveFromMap(MemMap_tasksMap, t);
    MemMap_Unlock(MemMap_tasksLock);
    put_task_struct(t);
}
