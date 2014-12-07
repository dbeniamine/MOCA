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
#define MEMMAP_TDATA_TABLE_SIZE 2*MEMMAP_TDATA_HASH_SIZE
#define MEMMAP_NB_CHUNKS 2//2*10

#define MEMMAP_VALID_CHUNKID(c) ( (c) >= 0 && (c) < MEMMAP_NB_CHUNKS )

#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "memmap.h"
#include "memmap_taskdata.h"
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
    spinlock_t lock;
}*task_data;

// Use by flush function to keep track of the last chunk
void MemMap_FlushData(task_data data);

task_data MemMap_InitData(struct task_struct *t,int id)
{
    int i,j;
    //We must not wait here !
    task_data data=kmalloc(sizeof(struct _task_data),GFP_ATOMIC);
    /* MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Initialising data for task %p\n",t); */
    if(!data)
    {
        MemMap_Panic("MemMap unable to allocate data ");
        return NULL;
    }
    for(i=0;i<MEMMAP_NB_CHUNKS;i++)
    {
        /* MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Initialising data chunk %d for task %p\n",i, t); */
        data->chunks[i]=kmalloc(sizeof(chunk),GFP_ATOMIC);
        if(!data->chunks[i])
        {
            MemMap_Panic("MemMap unable to allocate data chunk");
            return NULL;
        }
        data->chunks[i]->nbentry=0;
        for(j=0;j<MEMMAP_TDATA_TABLE_SIZE;j++)
        {
            data->chunks[i]->startClocks=
                kmalloc(sizeof(unsigned long long)*MemMap_NumThreads(), GFP_ATOMIC);
            data->chunks[i]->endClocks=
                kmalloc(sizeof(unsigned long long)*MemMap_NumThreads(), GFP_ATOMIC);
            if(!data->chunks[i]->startClocks || ! data->chunks[i]->startClocks)
            {
                MemMap_Panic("MemMap unable to allocate data chunk");
                return NULL;
            }
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
    data->internalId=id;
    get_task_struct(data->task);
    /* MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Initialising data chunks for task %p\n",t); */
    /* MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Initialising data lock for task %p\n",t); */
    spin_lock_init(&data->lock);
    /* MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Data ready for task %p\n",t); */
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
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap hashmap adding %p to chunk %d %p data %p cpu %d\n",
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
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap hashmap collision detected inserting %p hash %lu, ind %d\n", addr, h, ind);
        //Find a good spot
        while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
            ind=ch->table[ind].next_ind;
        //Addr already in the table
        if(ch->table[ind].addr==addr)
        {
            MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap addr already in chunk %p\n", addr);
            /* ++ch->table[ind].count; */
            return 0;
        }
        ch->table[ind].next_ind=ch->nbentry;
    }
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap inserting %p ind %d/%lu\n",
            addr,ch->nbentry,MEMMAP_TDATA_TABLE_SIZE);
    //Insertion in the table
    ch->table[ch->nbentry].addr=addr;
    ch->table[ch->nbentry].next_ind=-1;
    ch->table[ch->nbentry].cpu=1<<cpu;
    ch->cpu|=1<<cpu;
    ch->table[ch->nbentry].countR=0;
    ch->table[ch->nbentry].countW=0;
    ++ch->nbentry;
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap inserted %p\n", addr);
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
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Goto next chunks%p, %d, %d\n", data, data->cur, data->prev);
    MemMap_GetClocks(data->chunks[data->cur]->endClocks);
    data->cur=(data->cur+1)%MEMMAP_NB_CHUNKS;
    data->prev=(data->prev+1)%MEMMAP_NB_CHUNKS;
    if(data->cur==0)
    {
        MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Flushin chunks\n");
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
    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap Looking for next addr in ch %d, pos %d/%u-%lu\n",
            chunkid, pos, data->chunks[chunkid]->nbentry, MEMMAP_TDATA_TABLE_SIZE);
    if(pos < 0 ||pos >= data->chunks[chunkid]->nbentry)
    {
        MEMMAP_DEBUG_PRINT(KERN_WARNING "Nothing available\n");
        return NULL;
    }
    addr=data->chunks[chunkid]->table[pos].addr;
    MEMMAP_DEBUG_PRINT(KERN_WARNING "found adress %p\n", addr);
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


void MemMap_FlushData(task_data data)
{
    int chunkid, ind;
    unsigned long h;

    MEMMAP_DEBUG_PRINT(KERN_WARNING "MemMap_FlushData not implemented yet\n");
    for(chunkid=0; chunkid < MEMMAP_NB_CHUNKS;++chunkid)
    {
        //TODO: print clocks nbentry
        //taskid Chunk clocks nbentries cpumask
        for(ind=0; ind < data->chunks[chunkid]->nbentry;++ind)
        {
            //Access clock0 clock1 addr pagesize cpumask countread countwrite taskid
            /* MemMap_PrintEntry(data->chunks[chunkid]->table[ind]); */
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
    MemMap_GetClocks(data->chunks[0]->startClocks);
}
