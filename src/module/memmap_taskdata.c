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
#define MEMMAP_BUF_SIZE 2048
int MemMap_taskDataHashBits=14;
int MemMap_taskDataTableFactor=2;
int MemMap_nbChunks=20;

//TODO: fix that dynamically
#define MEMMAP_PAGE_SIZE 4096

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "memmap.h"
#include "memmap_taskdata.h"
#include "memmap_tasks.h"
#include "memmap_threads.h"
#include "memmap_hashmap.h"

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
    //We must not wait here !
    task_data data=kmalloc(sizeof(struct _task_data),GFP_ATOMIC);
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
    data->cur=0;
    MemMap_GetClocks(data->chunks[0]->startClocks);
    data->internalId=MemMap_nextTaskId++;
    data->nbflush=0;
    get_task_struct(data->task);
    /* MEMMAP_DEBUG_PRINT("MemMap Initialising data chunks for task %p\n",t); */
    /* MEMMAP_DEBUG_PRINT("MemMap Initialising data lock for task %p\n",t); */
    spin_lock_init(&data->lock);
    /* MEMMAP_DEBUG_PRINT("MemMap Data ready for task %p\n",t); */
    return data;
}

void MemMap_ClearData(task_data data)
{
    int i;
    if(!data)
        return;
    //TODO: Wait until all data are flushed
    //MemMap_FlushData(data);
    put_task_struct(data->task);
    for(i=0;i<MemMap_nbChunks;i++)
    {
        MemMap_FreeMap(data->chunks[i]->map);
        kfree(data->chunks[i]->endClocks);
        kfree(data->chunks[i]->startClocks);
        kfree(data->chunks[i]);
    }
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

void MemMap_PrintClocks(unsigned long long *clocks, char *buf)
{
    size_t size=MEMMAP_BUF_SIZE;
    int i,pos=0;
    if(!buf)
        return;
    buf[0]='{';
    for(i=0;i<MemMap_NumThreads();++i)
    {
        ++pos;
        size-=pos;
        pos+=snprintf(buf+pos, size, "%llu", clocks[i]);
        if(size==0)
        {
            buf[pos-1]='\0';
            return;
        }
        buf[pos]=',';
    }
    buf[pos]='}';
    if(size>1)
        pos++;
    buf[pos]='\0';
}

void MemMap_CpuMask(int cpu, char *buf)
{
    int i=0,pos=8*sizeof(int)-1, ok=0;
    if(!buf)
        return;
    while(pos>=0)
    {
        ok|=cpu&(1<<pos);
        if(ok)
        {
            buf[i]=(cpu&(1<<pos))?'1':'0';
            ++i;
        }
        --pos;
    }
    buf[i]='\0';
}

#define MemMap_Printk(...) \
    printk(KERN_NOTICE __VA_ARGS__);

void MemMap_FlushData(task_data data)
{
    int chunkid, ind, nelt;
    chunk_entry e;
    char *clk,*clk1, *CPUMASK;
    clk=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);
    clk1=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);
    CPUMASK=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);
    if(!clk1 || !clk || !CPUMASK)
        printk(KERN_WARNING "MemMap unable to allocate buffers in flush data\n");

    MemMap_Printk("==MemMap Taskdata %d %p %p\n", data->internalId,data->task, data);
    for(chunkid=0; chunkid < MemMap_nbChunks;++chunkid)
    {
        if((nelt=MemMap_NbElementInMap(data->chunks[chunkid]->map)))
        {
            MemMap_PrintClocks(data->chunks[chunkid]->startClocks,clk);
            MemMap_PrintClocks(data->chunks[chunkid]->endClocks,clk1);
            MemMap_CpuMask(data->chunks[chunkid]->cpu, CPUMASK);
            //Chunk chunkdid clock clock nbentry cpumask taskid
            MemMap_Printk("==MemMap Chunk %d %s %s %d 0b%s %d\n",
                    chunkid+data->nbflush*MemMap_nbChunks,clk,clk1,
                    nelt,CPUMASK, data->internalId
                    );
            ind=0;
            while((e=(chunk_entry)MemMap_EntryAtPos(data->chunks[chunkid]->map,ind)))
            {
                //Access clock0 clock1 addr pagesize cpumask countread countwrite chunkid taskid
                MemMap_CpuMask(e->cpu, CPUMASK);
                MemMap_Printk("==MemMap Access %s %s 0x%p 0x%x 0b%s %d %d %d %d\n",
                        clk,clk1,e->key, MEMMAP_PAGE_SIZE,CPUMASK, e->countR,
                        e->countW, chunkid+data->nbflush*MemMap_nbChunks,
                        data->internalId
                        );
                //Re init data
                MemMap_RemoveFromMap(data->chunks[chunkid]->map,e->key);
                e->countR=0;
                e->countW=0;
                e->cpu=0;
                ++ind;
            }
            data->chunks[chunkid]->cpu=0;
        }
    }
    ++data->nbflush;
    MemMap_GetClocks(data->chunks[0]->startClocks);
    //Free buffers
    if(clk)
        kfree(clk);
    if(clk1)
        kfree(clk1);
    if(CPUMASK)
        kfree(CPUMASK);
}
