/*
 * Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
 * Author: Beniamine, David <David@Beniamine.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define __NO_VERSION__
//#define MOCA_DEBUG





#include "moca.h"
#include "moca_tasks.h"



// The first bits are not random enough, 14 bits should be enough for pids
unsigned long Moca_tasksHashBits=14;
DEFINE_RWLOCK(Moca_tasksLock);

// Monitored process
hash_map Moca_tasksMap;
struct task_struct *Moca_initTask=NULL;

moca_task Moca_AddTask(struct task_struct *t);

void Moca_TaskInitializer(void *e)
{
    moca_task t=(moca_task)e;
    t->data=NULL;
}

int Moca_InitProcessManagment(int id)
{
    // Monitored pids
    struct pid *pid;
    rwlock_init(&Moca_tasksLock);
    Moca_tasksMap=Moca_InitHashMap(Moca_tasksHashBits,
            2*(1<<Moca_tasksHashBits), sizeof(struct _moca_task), NULL,
            Moca_TaskInitializer);
    rcu_read_lock();
    pid=find_vpid(id);
    if(!pid)
    {
        // Skip internal process
        rcu_read_unlock();
        Moca_Panic("Moca unable to find pid for init task");
        return 1;
    }
    Moca_initTask=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();
    Moca_InitTaskData();
    return 0;

}

void Moca_CleanProcessData(void)
{
    if(Moca_tasksMap)
    {
        Moca_ClearAllData();
        Moca_FreeMap(Moca_tasksMap);
    }
}

static inline int Moca_ShouldMonitorTask(struct task_struct *t)
{
    int ret=0;
    struct _moca_task tsk;
    if(t->real_parent==Moca_initTask)
        return 1;
    tsk.key=t->real_parent;
    read_lock(&Moca_tasksLock);
    ret=Moca_EntryFromKey(Moca_tasksMap, (hash_entry)&tsk)!=NULL;
    read_unlock(&Moca_tasksLock);
    return ret;

}

// Add pid to the monitored process if pid is a monitored process
moca_task Moca_AddTaskIfNeeded(struct task_struct *t)
{
    moca_task ret=NULL;
    if(t && pid_alive(t) && t->real_parent && Moca_ShouldMonitorTask(t))
        ret=Moca_AddTask(t);
    return ret;
}

// Current number of monitored pids
int Moca_GetNumTasks(void)
{
    int nb=0;
    read_lock(&Moca_tasksLock);
    nb=Moca_NbElementInMap(Moca_tasksMap);
    read_unlock(&Moca_tasksLock);
    return nb;
}

moca_task Moca_NextTask(int *pos)
{
    moca_task ret=NULL;
    MOCA_DEBUG_PRINT("Moca Looking for task at %d\n", *pos);
    read_lock(&Moca_tasksLock);
    ret=(moca_task)Moca_NextEntryPos(Moca_tasksMap,pos);
    MOCA_DEBUG_PRINT("Moca found task %p at %d\n", ret, *pos);
    read_unlock(&Moca_tasksLock);
    return ret;
}

task_data Moca_GetData(struct task_struct *t)
{
    int pos;
    struct _moca_task tsk;
    task_data ret=NULL;
    tsk.key=t;
    read_lock(&Moca_tasksLock);
    if((pos=Moca_PosInMap(Moca_tasksMap ,(hash_entry)&tsk))>=0)
        ret=((moca_task)Moca_EntryAtPos(Moca_tasksMap,pos))->data;
    read_unlock(&Moca_tasksLock);
    return ret;
}


// Add t to the monitored pids
moca_task Moca_AddTask(struct task_struct *t)
{
    task_data data;
    moca_task tsk;
    struct _moca_task tmptsk;
    int status;
    MOCA_DEBUG_PRINT("Moca Adding task %p\n",t );


    //Create the task data
    data=Moca_InitData(t);
    if(!data)
        return NULL;
    get_task_struct(t);

    tmptsk.key=t;
    tmptsk.data=data;
    write_lock(&Moca_tasksLock);
    tsk=(moca_task)Moca_AddToMap(Moca_tasksMap,(hash_entry)&tmptsk,&status);
    write_unlock(&Moca_tasksLock);
    switch(status)
    {
        case MOCA_HASHMAP_FULL:
            Moca_Panic("Moca Too many pids");
            break;
        case MOCA_HASHMAP_ERROR:
            Moca_Panic("Moca unhandeled hashmap error");
            break;
        case  MOCA_HASHMAP_ALREADY_IN_MAP:
            MOCA_DEBUG_PRINT("Moca Adding an already exixsting task %p\n", t);
            break;
        default:
            //normal add
            MOCA_DEBUG_PRINT("Moca Added task %p at pos %d \n", t, status);
            break;
    }
    return tsk;
}

void Moca_RemoveTask(struct task_struct *t)
{
    struct _moca_task tsk;
    tsk.key=t;
    write_lock(&Moca_tasksLock);
    Moca_RemoveFromMap(Moca_tasksMap, (hash_entry)&tsk);
    write_unlock(&Moca_tasksLock);
    put_task_struct(t);
}
