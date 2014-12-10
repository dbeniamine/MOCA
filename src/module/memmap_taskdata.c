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
#define MEMMAP_TDATA_HASH_BITS 14UL
#define MEMMAP_TDATA_HASH_SIZE (1UL<<MEMMAP_TDATA_HASH_BITS)
#define MEMMAP_TDATA_TABLE_SIZE (2*MEMMAP_TDATA_HASH_SIZE)
#define MEMMAP_NB_CHUNKS (2*10)
#define MEMMAP_BUF_SIZE 2048

#define MEMMAP_VALID_CHUNKID(c) ( (c) >= 0 && (c) < MEMMAP_NB_CHUNKS )
//TODO: fix that dynamically
#define MEMMAP_PAGE_SIZE 4096

#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "memmap.h"
#include "memmap_taskdata.h"
#include "memmap_tasks.h"
#include "memmap_threads.h"

typedef struct
{
    void *addr;
    int countR;
    int countW;
    int next_ind;
    int cpu;
}chunk_entry;

typedef struct
{
    chunk_entry table[MEMMAP_TDATA_TABLE_SIZE];
    unsigned int nbentry;
    int hashs[MEMMAP_TDATA_HASH_SIZE];
    unsigned long long *startClocks;
    unsigned long long *endClocks;
    int cpu;
}chunk;

typedef struct _task_data
{
    struct task_struct *task;
    chunk *chunks[MEMMAP_NB_CHUNKS];
    int cur;
    int prev;
    int internalId;
    int nbflush;
    spinlock_t lock;
}*task_data;

// Use by flush function to keep track of the last chunk
void MemMap_FlushData(task_data data);
int MemMap_nextTaskId=0;

task_data MemMap_InitData(struct task_struct *t)
{
    int i,j;
    //We must not wait here !
    task_data data=kmalloc(sizeof(struct _task_data),GFP_ATOMIC);
    /* MEMMAP_DEBUG_PRINT("MemMap Initialising data for task %p\n",t); */
    if(!data)
    {
        MemMap_Panic("MemMap unable to allocate data ");
        return NULL;
    }
    for(i=0;i<MEMMAP_NB_CHUNKS;i++)
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
        data->chunks[i]->nbentry=0;
        for(j=0;j<MEMMAP_TDATA_TABLE_SIZE;j++)
        {
            data->chunks[i]->table[j].addr=NULL;
            data->chunks[i]->table[j].countR=0;
            data->chunks[i]->table[j].countW=0;
            data->chunks[i]->table[j].next_ind=-1;
            data->chunks[i]->table[j].cpu=0;
            if(j<MEMMAP_TDATA_HASH_SIZE)
                data->chunks[i]->hashs[j]=-1;
        }


    }
    data->task=t;
    data->cur=0;
    MemMap_GetClocks(data->chunks[0]->startClocks);
    data->prev=-1;
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
    MemMap_FlushData(data);
    put_task_struct(data->task);
    for(i=0;i<MEMMAP_NB_CHUNKS;i++)
    {
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

int MemMap_AddToChunk(task_data data, void *addr, int cpu,int chunkid)
{
    unsigned long h=hash_ptr(addr,MEMMAP_TDATA_HASH_BITS);
    chunk *ch;
    int ind;
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return -1;
    ch=data->chunks[chunkid];
    MEMMAP_DEBUG_PRINT("MemMap hashmap adding %p to chunk %d %p data %p cpu %d\n",
            addr, chunkid, ch,data, cpu);
    if(ch->nbentry>=MEMMAP_TDATA_TABLE_SIZE)
    {
        MemMap_Panic("MemMap have too many adresses in chunk");
        return -2;
    }

    if(ch->hashs[h]==-1)
    {
        ch->hashs[h]=ch->nbentry;
    }
    else
    {
        //Conflict detected
        ind=ch->hashs[h];
        MEMMAP_DEBUG_PRINT("MemMap hashmap collision detected inserting %p hash %lu, ind %d\n", addr, h, ind);
        //Find a good spot
        while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
            ind=ch->table[ind].next_ind;
        //Addr already in the table
        if(ch->table[ind].addr==addr)
        {
            MEMMAP_DEBUG_PRINT("MemMap addr already in chunk %p\n", addr);
            /* ++ch->table[ind].count; */
            return 0;
        }
        ch->table[ind].next_ind=ch->nbentry;
    }
    MEMMAP_DEBUG_PRINT("MemMap inserting %p ind %d/%lu\n",
            addr,ch->nbentry,MEMMAP_TDATA_TABLE_SIZE);
    //Insertion in the table
    ch->table[ch->nbentry].addr=addr;
    ch->table[ch->nbentry].next_ind=-1;
    ch->table[ch->nbentry].cpu|=1<<cpu;
    ch->cpu|=1<<cpu;
    ch->table[ch->nbentry].countR=0;
    ch->table[ch->nbentry].countW=0;
    ++ch->nbentry;
    MEMMAP_DEBUG_PRINT("MemMap inserted %p\n", addr);
    return 0;
}

int MemMap_IsInChunk(task_data data, void *addr, int chunkid)
{
    unsigned long h=hash_ptr(addr,MEMMAP_TDATA_HASH_BITS);
    chunk *ch;
    int ind;
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return 0;
    ch=data->chunks[chunkid];
    ind=ch->hashs[h];
    if(ind==-1)
        return 0;
    while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
        ind=ch->table[ind].next_ind;
    return (ch->table[ind].addr==addr);
}

int MemMap_UpdateData(task_data data,int pos, int countR, int countW, int chunkid, int cpu)
{
    chunk *ch;
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return -1;
    ch=data->chunks[chunkid];
    if(pos<0 || pos > ch->nbentry)
        return 1;
    ch->table[pos].countR+=countR;
    ch->table[pos].countW+=countW;
    ch->table[pos].cpu|=1<<cpu;
    ch->cpu|=1<<cpu;
    return 0;
}

int MemMap_CurrentChunk(task_data data)
{
    return data->cur;
}
int MemMap_PreviousChunk(task_data data)
{
    return data->prev;
}

// Set current chunk as prev and clear current
int MemMap_NextChunks(task_data data)
{
    MEMMAP_DEBUG_PRINT("MemMap Goto next chunks %p, %d, %d\n", data, data->cur, data->prev);
    MemMap_GetClocks(data->chunks[data->cur]->endClocks);
    data->prev=data->cur;
    data->cur=(data->cur+1)%MEMMAP_NB_CHUNKS;
    MEMMAP_DEBUG_PRINT("MemMap Goto chunks  %p %d, %d/%d\n", data, data->cur, data->prev, MEMMAP_NB_CHUNKS);
    if(data->cur==0)
    {
        MEMMAP_DEBUG_PRINT("MemMap Flushin chunks\n");
        MemMap_FlushData(data);
        return 1;
    }
    else
        MemMap_GetClocks(data->chunks[data->cur]->startClocks);
    return 0;
}


void *MemMap_AddrInChunkPos(task_data data,int pos, int chunkid)
{
    void *addr;
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return NULL;
    MEMMAP_DEBUG_PRINT("MemMap Looking for next addr in ch %d, pos %d/%u-%lu\n",
            chunkid, pos, data->chunks[chunkid]->nbentry, MEMMAP_TDATA_TABLE_SIZE);
    if(pos < 0 ||pos >= data->chunks[chunkid]->nbentry)
    {
        MEMMAP_DEBUG_PRINT("Nothing available\n");
        return NULL;
    }
    addr=data->chunks[chunkid]->table[pos].addr;
    MEMMAP_DEBUG_PRINT("found adress %p\n", addr);
    return addr;
}

void MemMap_LockData(task_data data)
{
    spin_lock(&data->lock);
}
void MemMap_unLockData(task_data data)
{
    spin_unlock(&data->lock);
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
    int chunkid, ind;
    unsigned long h;
    char *clk,*clk1, *CPUMASK;
    clk=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);
    clk1=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);
    CPUMASK=kmalloc(MEMMAP_BUF_SIZE,GFP_ATOMIC);
    if(!clk1 || !clk || !CPUMASK)
        printk(KERN_WARNING "MemMap unable to allocate buffers in flush data\n");

    MemMap_Printk(KERN_NOTICE "==MemMap Taskdata %d %p %p\n", data->internalId,data->task, data);
    for(chunkid=0; chunkid < MEMMAP_NB_CHUNKS;++chunkid)
    {
        if(data->chunks[chunkid]->nbentry)
        {
            MemMap_PrintClocks(data->chunks[chunkid]->startClocks,clk);
            MemMap_PrintClocks(data->chunks[chunkid]->endClocks,clk1);
            MemMap_CpuMask(data->chunks[chunkid]->cpu, CPUMASK);
            //Chunk chunkdid clock clock nbentry cpumask taskid
            MemMap_Printk(KERN_NOTICE "==MemMap Chunk %d %s %s %d 0b%s %d\n",
                    chunkid+data->nbflush*MEMMAP_NB_CHUNKS,clk,clk1,
                    data->chunks[chunkid]->nbentry,CPUMASK, data->internalId
                  );
            //TODO: print clocks nbentry
            for(ind=0; ind < data->chunks[chunkid]->nbentry;++ind)
            {
                //Access clock0 clock1 addr pagesize cpumask countread countwrite chunkid taskid
                MemMap_CpuMask(data->chunks[chunkid]->table[ind].cpu, CPUMASK);
                MemMap_Printk(KERN_NOTICE "==MemMap Access %s %s 0x%p 0x%x 0b%s %d %d %d %d\n",
                        clk,clk1,data->chunks[chunkid]->table[ind].addr,
                        MEMMAP_PAGE_SIZE,CPUMASK,
                        data->chunks[chunkid]->table[ind].countR,
                        data->chunks[chunkid]->table[ind].countW,
                        chunkid+data->nbflush*MEMMAP_NB_CHUNKS, data->internalId
                      );
                //Re init data
                h=hash_ptr(data->chunks[chunkid]->table[ind].addr,MEMMAP_TDATA_HASH_BITS);
                data->chunks[chunkid]->hashs[h]=-1;
                data->chunks[chunkid]->table[ind].addr=NULL;
                data->chunks[chunkid]->table[ind].countR=0;
                data->chunks[chunkid]->table[ind].countW=0;
                data->chunks[chunkid]->table[ind].next_ind=-1;
            }
            data->chunks[chunkid]->cpu=0;
            data->chunks[chunkid]->nbentry=0;
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
