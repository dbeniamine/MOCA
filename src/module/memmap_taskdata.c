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
#define MEMMAP_BUF_SIZE 4096
int MemMap_taskDataHashBits=14;
int MemMap_taskDataTableFactor=2;
int MemMap_nbChunks=20;

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>  /* for copy_*_user */
#include <linux/delay.h>
#include "memmap.h"
#include "memmap_taskdata.h"
#include "memmap_tasks.h"
#include "memmap_threads.h"
#include "memmap_hashmap.h"

//Create_proc_entry doesn't exist since linux 3.10.0

/* #if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) */
/* #define proc_create(name,mode, parent,proc_fops) \ */
/*     proc_create(name,mode,parent,proc_fops)->read_proc=fct */
/* #endif */
static struct proc_dir_entry *MemMap_proc_root;
static ssize_t MemMap_FlushData(struct file *filp,  char *buffer,
        size_t length, loff_t * offset);

static const struct file_operations MemMap_taskdata_fops = {
    .owner   = THIS_MODULE,
    .read    = MemMap_FlushData,
};

typedef struct _chunk_entry
{
    void *key; //Address
    int next;
    int countR;
    int countW;
    int cpu;
}*chunk_entry;

typedef struct
{
    hash_map map;
    unsigned long long *startClocks;
    unsigned long long *endClocks;
    int cpu;
    int used;
    spinlock_t lock;
}chunk;

typedef struct _task_data
{
    struct task_struct *task;
    chunk **chunks;
    int cur;
    int internalId;
    int nbflush;
    spinlock_t lock;
    struct proc_dir_entry *proc_entry;
}*task_data;

int MemMap_nextTaskId=0;

int MemMap_CurrentChunk(task_data data)
{
    int ret=-1;
    spin_lock(&data->lock);
    ret=data->cur;
    spin_unlock(&data->lock);
    return ret;
}


task_data MemMap_InitData(struct task_struct *t)
{
    int i;
    task_data data;
    char buf[10];
    //Procfs init
    if(MemMap_nextTaskId==0)
    {
        //First call
        MemMap_proc_root=proc_mkdir("MemMap", NULL);
        if(!MemMap_proc_root)
        {
            MemMap_Panic("MemMap Unable to create proc root entry");
            return NULL;
        }
    }
    //We must not wait here !
    data=kmalloc(sizeof(struct _task_data),GFP_ATOMIC);
    /* MEMMAP_DEBUG_PRINT("MemMap Initialising data for task %p\n",t); */
    if(!data)
    {
        MemMap_Panic("MemMap unable to allocate data ");
        return NULL;
    }
    data->chunks=kmalloc(sizeof(chunk)*MemMap_nbChunks,GFP_ATOMIC);
    if(!data->chunks)
    {
        kfree(data);
        MemMap_Panic("MemMap unable to allocate chunks");
        return NULL;
    }
    for(i=0;i<MemMap_nbChunks;i++)
    {
        /* MEMMAP_DEBUG_PRINT("MemMap Initialising data chunk %d for task %p\n",i, t); */
        data->chunks[i]=kmalloc(sizeof(chunk),GFP_ATOMIC);
        if(!data->chunks[i])
        {
            MemMap_Panic("MemMap unable to allocate data chunk");
            return NULL;
        }
        data->chunks[i]->startClocks=
            kcalloc(MemMap_NumThreads(),sizeof(unsigned long long), GFP_ATOMIC);
        data->chunks[i]->endClocks=
            kcalloc(MemMap_NumThreads(),sizeof(unsigned long long), GFP_ATOMIC);
        if(!data->chunks[i]->startClocks || ! data->chunks[i]->startClocks)
        {
            MemMap_Panic("MemMap unable to allocate data chunk");
            return NULL;
        }
        data->chunks[i]->cpu=0;
        data->chunks[i]->used=0;
        data->chunks[i]->map=MemMap_InitHashMap(MemMap_taskDataHashBits,
                MemMap_taskDataTableFactor, sizeof(struct _chunk_entry));
        spin_lock_init(&data->chunks[i]->lock);
    }
    data->task=t;
    get_task_struct(data->task);
    data->cur=0;
    MemMap_GetClocks(data->chunks[0]->startClocks);
    data->internalId=MemMap_nextTaskId++;
    data->nbflush=0;
    snprintf(buf,10,"task%d",MemMap_nextTaskId);
    data->proc_entry=proc_create_data(buf,0,MemMap_proc_root,
            &MemMap_taskdata_fops,data);
    /* MEMMAP_DEBUG_PRINT("MemMap Initialising data chunks for task %p\n",t); */
    /* MEMMAP_DEBUG_PRINT("MemMap Initialising data lock for task %p\n",t); */
    spin_lock_init(&data->lock);
    /* MEMMAP_DEBUG_PRINT("MemMap Data ready for task %p\n",t); */
    return data;
}

void MemMap_ClearAllData(void)
{
    int i, nbTasks;
    MEMMAP_DEBUG_PRINT("MemMap Cleaning data\n");
    nbTasks=MemMap_GetNumTasks();
    for(i=0;i<nbTasks;++i)
    {
        MemMap_ClearData(((memmap_task)
                    MemMap_EntryAtPos(MemMap_tasksMap, (unsigned)i))->data);
    }
    proc_remove(MemMap_proc_root);
    MemMap_FreeMap(MemMap_tasksMap);
    MEMMAP_DEBUG_PRINT("MemMap Cleaning all data\n");
}

void MemMap_ClearData(task_data data)
{
    int i;
    if(!data)
        return;
    for(i=0;i<MemMap_nbChunks;i++)
    {
        // Wait for the data to be outputed
        while(data->chunks[i]->used!=0)
        {
            msleep(10);
        }
        MemMap_FreeMap(data->chunks[i]->map);
        kfree(data->chunks[i]->endClocks);
        kfree(data->chunks[i]->startClocks);
        kfree(data->chunks[i]);
    }
    put_task_struct(data->task);
    proc_remove(data->proc_entry);
    kfree(data);
}


struct task_struct *MemMap_GetTaskFromData(task_data data)
{
    return data->task;
}

int MemMap_AddToChunk(task_data data, void *addr, int cpu)
{
    int status, cur;
    chunk_entry e;
    cur=MemMap_CurrentChunk(data);
    spin_lock(&data->chunks[cur]->lock);
    if(data->chunks[cur]->used)
    {
        spin_unlock(&data->chunks[cur]->lock);
        return -1;
    }
    MEMMAP_DEBUG_PRINT("MemMap hashmap adding %p to chunk %d %p data %p cpu %d\n",
            addr, cur, data->chunks[cur],data, cpu);
    e=(chunk_entry)MemMap_AddToMap(data->chunks[cur]->map,addr, &status);
    switch(status)
    {
        case MEMMAP_HASHMAP_FULL :
            MemMap_Panic("MemMap hashmap full");
            spin_unlock(&data->chunks[cur]->lock);
            return -1;
            break;
        case MEMMAP_HASHMAP_ERROR :
            MemMap_Panic("MemMap hashmap error");
            spin_unlock(&data->chunks[cur]->lock);
            return -1;
            break;
        case MEMMAP_HASHMAP_ALREADY_IN_MAP :
            MEMMAP_DEBUG_PRINT("MemMap addr already in chunk %p\n", addr);
            e->cpu|=1<<cpu;
            ++e->countR;
            ++e->countW;
            break;
        default :
            //Normal add
            e->cpu=1<<cpu;
            e->countR=0;
            e->countW=0;
            break;
    }
    data->chunks[cur]->cpu|=1<<cpu;
    spin_unlock(&data->chunks[cur]->lock);
    MEMMAP_DEBUG_PRINT("MemMap inserted %p\n", addr);
    return 0;
}

/* int MemMap_IsInChunk(task_data data, void *addr) */
/* { */
/*     return (MemMap_PosInMap(data->chunks[data->cur]->map,addr))!=-1; */
/* } */

int MemMap_UpdateData(task_data data,int pos, int countR, int countW, int cpu)
{
    chunk_entry e;
    int cur=MemMap_CurrentChunk(data);
    spin_lock(&data->chunks[cur]->lock);
    if(data->chunks[cur]->used)
    {
        spin_unlock(&data->chunks[cur]->lock);
        return -1;
    }
    e=(chunk_entry )MemMap_EntryAtPos(data->chunks[cur]->map,
            pos);
    if(!e)
    {
        spin_unlock(&data->chunks[cur]->lock);
        return 1;
    }

    e->countR+=countR;
    e->countW+=countW;
    e->cpu|=1<<cpu;
    spin_unlock(&data->chunks[cur]->lock);
    return 0;
}

int MemMap_NextChunks(task_data data)
{
    int cur;
    MEMMAP_DEBUG_PRINT("MemMap Goto next chunks %p, %d\n", data, data->cur);
    //Global lock
    spin_lock(&data->lock);
    cur=data->cur;
    spin_lock(&data->chunks[cur]->lock);
    MemMap_GetClocks(data->chunks[cur]->endClocks);
    data->chunks[cur]->used=1;
    data->cur=(cur+1)%MemMap_nbChunks;
    spin_unlock(&data->chunks[cur]->lock);
    spin_unlock(&data->lock);
    MEMMAP_DEBUG_PRINT("MemMap Goto chunks  %p %d, %d\n", data, data->cur,
            MemMap_nbChunks);
    if(data->chunks[data->cur]->used)
    {
        printk(KERN_ALERT "MemMap no more chunks, stopping trace for task %d\n You can fix that by relaunching MemMap either with a higher number of chunks\n or by decreasing the logging daemon wakeupinterval\n",
                data->internalId);
        return 1;
    }
    else
        MemMap_GetClocks(data->chunks[data->cur]->startClocks);
    return 0;
}


void *MemMap_AddrInChunkPos(task_data data,int pos)
{
    chunk_entry e;
    int cur=MemMap_CurrentChunk(data);
    spin_lock(&data->chunks[cur]->lock);
    if(data->chunks[cur]->used)
    {
        spin_unlock(&data->chunks[cur]->lock);
        return NULL;
    }
    MEMMAP_DEBUG_PRINT("MemMap Looking for next addr in ch %d, pos %d/%u\n",
            cur, pos, MemMap_NbElementInMap(data->chunks[cur]->map));
    e=(chunk_entry)MemMap_EntryAtPos(data->chunks[cur]->map,
            pos);
    spin_unlock(&data->chunks[cur]->lock);
    if(!e)
        return NULL;
    MEMMAP_DEBUG_PRINT("found adress %p\n", e->key);
    return e->key;
}

int MemMap_PrintClocks(unsigned long long *clocks, char *buf, size_t size)
{
    int i,pos=0;
    if(!buf)
        return 0;
    buf[0]='{';
    for(i=0;i<MemMap_NumThreads();++i)
    {
        ++pos;
        size-=pos;
        pos+=snprintf(buf+pos, size, "%llu", clocks[i]);
        if(size==0)
        {
            MemMap_Panic("MemMap Buffer overflow in print clocks");
            return 0;
        }
        buf[pos]=',';
    }
    buf[pos]='}';
    return pos;
}

int MemMap_CpuMask(int cpu, char *buf, size_t size)
{
    int i=0,pos=8*sizeof(int)-1, ok=0;
    if(!buf)
        return 0;
    while(pos>=0 && i< size)
    {
        ok|=cpu&(1<<pos);
        if(ok)
        {
            buf[i]=(cpu&(1<<pos))?'1':'0';
            ++i;
        }
        --pos;
    }
    if( i>=size)
    {
        MemMap_Panic("MemMap Buffer overflow in CpuMask");
        return 0;
    }
    return i-1;
}

static ssize_t MemMap_FlushData(struct file *filp,  char *buffer,
        size_t length, loff_t * offset)
{
    ssize_t len=0,sz;
    int chunkid, ind, nelt;
    char *MYBUFF;
    chunk_entry e;
    task_data data=(task_data)PDE_DATA(file_inode(filp));
    MYBUFF=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);

    if(!MYBUFF)
        printk(KERN_WARNING "MemMap unable to allocate buffer in flush data\n");

    if(data->nbflush==0)
    {
        snprintf(MYBUFF,MEMMAP_BUF_SIZE,"Taskdata %d %p%p\n", data->internalId,data->task, data);
        len+=copy_to_user(buffer, MYBUFF,MEMMAP_BUF_SIZE);
    }
    for(chunkid=0; chunkid < MemMap_nbChunks;++chunkid)
    {
        spin_lock(&data->chunks[chunkid]->lock);
        if( data->chunks[chunkid]->used &&
                (nelt=MemMap_NbElementInMap(data->chunks[chunkid]->map)))
        {
            //TODO fix chunkid
            //Chunk id  nb element startclock endclock cpumask
            sz=snprintf(MYBUFF,MEMMAP_BUF_SIZE,"Chunk %d %d ",
                    chunkid+data->nbflush*MemMap_nbChunks,
                    MemMap_NbElementInMap(data->chunks[chunkid]->map));
            sz+=MemMap_PrintClocks(data->chunks[chunkid]->startClocks,MYBUFF+sz,
                    MEMMAP_BUF_SIZE-sz);
            MYBUFF[sz++]=' ';
            sz+=MemMap_PrintClocks(data->chunks[chunkid]->endClocks,MYBUFF+sz,
                    MEMMAP_BUF_SIZE-sz);
            MYBUFF[sz++]=' ';
            sz+=MemMap_CpuMask(data->chunks[chunkid]->cpu, MYBUFF+sz,
                    MEMMAP_BUF_SIZE-sz);
            MYBUFF[sz++]='\n';
            len+=copy_to_user(buffer+len,MYBUFF,sz);
            ind=0;
            while((e=(chunk_entry)MemMap_EntryAtPos(data->chunks[chunkid]->map,ind)))
            {
                //Access pte countread countwrite cpumask
                sz=snprintf(MYBUFF,MEMMAP_BUF_SIZE,"Access %p %d %d",
                        e->key, e->countR, e->countW);
                sz+=MemMap_CpuMask(e->cpu,MYBUFF+sz,MEMMAP_BUF_SIZE-sz);
                MYBUFF[sz++]='\n';
                len+=copy_to_user(buffer+len,MYBUFF,sz);
                //Re init data
                MemMap_RemoveFromMap(data->chunks[chunkid]->map,e->key);
                e->countR=0;
                e->countW=0;
                e->cpu=0;
                ++ind;
            }
            data->chunks[chunkid]->cpu=0;
            data->chunks[chunkid]->used=0;
        }
        spin_unlock(&data->chunks[chunkid]->lock);
    }
    ++data->nbflush;
    MemMap_GetClocks(data->chunks[0]->startClocks);
    //Free buffers
    if(MYBUFF)
        kfree(MYBUFF);
    return 0;
}
