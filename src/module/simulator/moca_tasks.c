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
rwlock_t Moca_tasksLock;

// Monitored process
hash_map Moca_tasksMap=NULL;
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
    if(!(Moca_tasksMap=Moca_InitHashMap(Moca_tasksHashBits,
            2*(1<<Moca_tasksHashBits), sizeof(struct _moca_task), NULL,
            Moca_TaskInitializer)))
        goto fail;
    rcu_read_lock();
    pid=find_vpid(id);
    if(!pid)
    {
        // Skip internal process
        rcu_read_unlock();
        goto clean;
    }
    Moca_initTask=pid_task(pid, PIDTYPE_PID);
    rcu_read_unlock();
    if(Moca_InitTaskData()!=0)
        goto clean;
    return 0;
clean:
    Moca_FreeMap(Moca_tasksMap);
fail:
    printk(KERN_NOTICE "Moca fail initializing process data \n");
    return 1;

}

void Moca_CleanProcessData(void)
{
    if(Moca_tasksMap)
    {
        Moca_ClearAllData();
        Moca_FreeMap(Moca_tasksMap);
        Moca_tasksMap=NULL;
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

    get_task_struct(t);
    tmptsk.key=t;
    write_lock(&Moca_tasksLock);
    tsk=(moca_task)Moca_AddToMap(Moca_tasksMap,(hash_entry)&tmptsk,&status);
    switch(status)
    {
        case MOCA_HASHMAP_FULL:
            printk(KERN_NOTICE "Moca too many tasks ignoring %p",t);
            goto fail;
        case MOCA_HASHMAP_ERROR:
            printk("Moca unhandeled hashmap error");
            goto fail;
        case  MOCA_HASHMAP_ALREADY_IN_MAP:
            MOCA_DEBUG_PRINT("Moca Adding an already exixsting task %p\n", t);
            return tsk;
        default:
            //normal add
            MOCA_DEBUG_PRINT("Moca Added task %p at pos %d \n", t, status);
            break;
    }
    // Here we are sure that t has been added to the map
    data=Moca_InitData(t);
    if(!data)
    {
        Moca_RemoveTask(t);
        goto fail;
    }
    tsk->data=data;
    write_unlock(&Moca_tasksLock);
    return tsk;
fail:
    write_unlock(&Moca_tasksLock);
    return NULL;
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
