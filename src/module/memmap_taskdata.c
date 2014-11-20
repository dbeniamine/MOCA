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
//Number of chunks, when all chunks are
#define MEMMAP_NB_CHUNKS 2*10

#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include "memmap.h"
#include "memmap_taskdata.h"

typedef struct
{
    void *addr;
    int type;
    int count;
    int cpu; //cpu on witch addr have been accessed
    int next_ind;
}chunk_entry;

typedef struct
{
    chunk_entry table[MEMMAP_TDATA_TABLE_SIZE];
    int nbentry;
    int hashs[MEMMAP_TDATA_HASH_SIZE];
    unsigned long long *clocks;
}chunk;

typedef struct _task_data
{
    struct task_struct *task;
    chunk chunks[MEMMAP_NB_CHUNKS];
    int cur;
    int prev;
    spinlock_t lock;
}*task_data;

void MemMap_FlushData(task_data data);

task_data MemMap_InitData(struct task_struct *t)
{
    task_data data=kcalloc(1,sizeof(struct _task_data),GFP_KERNEL);
    int i,j;
    if(!data)
    {
        MemMap_Panic("MemMap unable to allocate data");
        return NULL;
    }
    data->task=t;
    data->cur=0;
    data->prev=1;
    get_task_struct(data->task);
    for(i=0;i<MEMMAP_NB_CHUNKS;i++)
    {
        data->chunks[i].nbentry=0;
        for(j=0;j<MEMMAP_TDATA_HASH_SIZE;j++)
            data->chunks[i].hashs[j]=-1;
    }
    spin_lock_init(&data->lock);
    return data;
}

void MemMap_ClearData(task_data data)
{
    if(!data)
        return;
    MemMap_FlushData(data);
    put_task_struct(data->task);
    kfree(data);
}


struct task_struct *MemMap_GetTaskFromData(task_data data)
{
    return data->task;
}

void MemMap_AddToChunk(task_data data, void *addr,int cpu, int chunkid)
{
    unsigned long h=hash_ptr(addr,MEMMAP_TDATA_HASH_BITS);
    chunk *ch=&(data->chunks[chunkid]);
    int ind;
    if(ch->nbentry>=MEMMAP_TDATA_TABLE_SIZE)
    {
        MemMap_Panic("MemMap have too many adresses in chunk");
        return;
    }

    if(ch->hashs[h]==-1)
    {
        ch->hashs[h]=ch->nbentry;
    }
    else
    {
        //Conflict detected
        printk(KERN_WARNING "MemMap hashmap collision detected inserting %p\n",
                addr);
        //Find a good spot
        ind=ch->hashs[h];
        while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
            ind=ch->table[ind].next_ind;
        //Addr already in the table
        if(ch->table[ind].addr==addr)
            return;
        ch->table[ind].next_ind=ch->nbentry;
    }
    //Insertion in the table
    ch->table[ch->nbentry].addr=addr;
    ch->table[ch->nbentry].cpu=cpu;
    ch->table[ch->nbentry].next_ind=-1;
    ch->table[ch->nbentry].type=MEMMAP_ACCESS_NONE;
    ch->table[ch->nbentry].count=0;
    ++ch->nbentry;
}

int MemMap_IsInChunk(task_data data, void *addr, int chunkid)
{
    unsigned long h=hash_ptr(addr,MEMMAP_TDATA_HASH_BITS);
    chunk *ch=&(data->chunks[chunkid]);
    int ind=ch->hashs[h];
    if(ind==-1)
        return 1;
    while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
        ind=ch->table[ind].next_ind;
    return (ch->table[ind].addr==addr);
}

int MemMap_UpdateAdressData(task_data data,void *addr, int type,int count)
{
    unsigned long h=hash_ptr(addr,MEMMAP_TDATA_HASH_BITS);
    chunk *ch=&(data->chunks[data->cur]);
    int ind=ch->hashs[h];
    if(ind==-1)
        return 1;
    while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
        ind=ch->table[ind].next_ind;
    if (ch->table[ind].addr!=addr)
        return 1;
    ch->table[ind].type&=type;
    ch->table[ind].count+=count;
    return 0;
}

// Set current chunk as prev and clear current
int MemMap_NextChunks(task_data data, unsigned long long *clocks)
{
    ++data->cur;
    ++data->prev;
    data->chunks[data->cur].clocks=clocks;
    if(data->cur==MEMMAP_NB_CHUNKS)
    {
        MemMap_FlushData(data);
        return 1;
    }
    return 0;
}


void *MemMap_NextAddrInChunk(task_data data,int *pos, int chunkid)
{
    void *addr;
    if(*pos >= data->chunks[chunkid].nbentry)
        return NULL;
    addr=data->chunks[chunkid].table[*pos].addr;
    ++*pos;
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
    //TODO
    // Don't forget to kfree(data->chunks[i]clocks);
}
