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
#include <asm/page.h>
#include <asm/pgtable.h>
#include "memmap.h"
#include "memmap_taskdata.h"

typedef struct
{
    void *addr;
    int type;
    int count;
    int next_ind;
    int cpu;
}chunk_entry;

typedef struct
{
    chunk_entry table[MEMMAP_TDATA_TABLE_SIZE];
    unsigned int nbentry;
    int hashs[MEMMAP_TDATA_HASH_SIZE];
    unsigned long long *clocks;
}chunk;

typedef struct _task_data
{
    struct task_struct *task;
    chunk *chunks[MEMMAP_NB_CHUNKS];
    int cur;
    int prev;
    spinlock_t lock;
}*task_data;

void MemMap_FlushData(task_data data);

task_data MemMap_InitData(struct task_struct *t)
{
    int i,j;
    //We must not wait here !
    task_data data=kmalloc(sizeof(struct _task_data),GFP_ATOMIC);
    printk(KERN_WARNING "MemMap Trying to malloc size %lu\n", sizeof(struct _task_data));
    printk(KERN_WARNING "MemMap Initialising data for task %p\n",t);
    if(!data)
    {
        MemMap_Panic("MemMap unable to allocate data ");
        return NULL;
    }
    for(i=0;i<MEMMAP_NB_CHUNKS;i++)
    {
        printk(KERN_WARNING "MemMap Initialising data chunk %d for task %p\n",i, t);
        data->chunks[i]=kmalloc(sizeof(chunk),GFP_ATOMIC);
        if(!data->chunks[i])
        {
            MemMap_Panic("MemMap unable to allocate data chunk");
            return NULL;
        }
        data->chunks[i]->nbentry=0;
        for(j=0;j<MEMMAP_TDATA_HASH_SIZE;j++)
            data->chunks[i]->hashs[j]=-1;


    }
    data->task=t;
    data->cur=0;
    data->prev=-1;
    get_task_struct(data->task);
    printk(KERN_WARNING "MemMap Initialising data chunks for task %p\n",t);
    printk(KERN_WARNING "MemMap Initialising data lock for task %p\n",t);
    spin_lock_init(&data->lock);
    printk(KERN_WARNING "MemMap Data ready for task %p\n",t);
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
        kfree(data->chunks[i]);
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
    printk(KERN_WARNING "MemMap hashmap addin %p to chunk %d cpu %d\n", addr, chunkid, cpu);
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return -1;
    ch=data->chunks[chunkid];
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
        printk(KERN_WARNING "MemMap hashmap collision detected inserting %p hash %lu, ind %d\n", addr, h, ind);
        //Find a good spot
        while(ch->table[ind].addr!=addr && ch->table[ind].next_ind!=-1)
            ind=ch->table[ind].next_ind;
        //Addr already in the table
        if(ch->table[ind].addr==addr)
        {
            printk(KERN_WARNING "MemMap addr already in chunk %p\n", addr);
            return 0;
        }
        ch->table[ind].next_ind=ch->nbentry;
    }
    printk(KERN_WARNING "MemMap inserting %p ind %d/%lu\n",
            addr,ch->nbentry,MEMMAP_TDATA_TABLE_SIZE);
    //Insertion in the table
    ch->table[ch->nbentry].addr=addr;
    ch->table[ch->nbentry].next_ind=-1;
    ch->table[ch->nbentry].cpu=cpu;
    ch->table[ch->nbentry].type=MEMMAP_ACCESS_NONE;
    ch->table[ch->nbentry].count=0;
    ++ch->nbentry;
    printk(KERN_WARNING "MemMap inserted %p\n", addr);
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

int MemMap_UpdateAdressData(task_data data,void *addr, int type,int count,
        int chunkid)
{
    unsigned long h=hash_ptr(addr,MEMMAP_TDATA_HASH_BITS);
    chunk *ch;
    int ind;
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return -1;
    ch=data->chunks[chunkid];
    ind=ch->hashs[h];
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

int MemMap_CurrentChunk(task_data data)
{
    return data->cur;
}
int MemMap_PreviousChunk(task_data data)
{
    return data->prev;
}

// Set current chunk as prev and clear current
int MemMap_NextChunks(task_data data, unsigned long long *clocks)
{
    int i;
    pte_t *pte;
    printk(KERN_WARNING "MemMap Goto next chunks%p, %d, %d\n", data, data->cur, data->prev);
    data->chunks[data->cur]->clocks=clocks;
    // Mark absent all pte in previous chunk, should be done somewhere else
    if(MEMMAP_VALID_CHUNKID(data->prev))
    {
        for(i=0;i<data->chunks[data->prev]->nbentry;++i)
        {
            pte=(pte_t *)(data->chunks[data->prev]->table[i].addr);
            if(pte_present(*pte) && !pte_none(*pte))
                *pte=pte_clear_flags(*pte,_PAGE_PRESENT);
        }
    }
    data->cur=(data->cur+1)%MEMMAP_NB_CHUNKS;
    data->prev=(data->prev+1)%MEMMAP_NB_CHUNKS;
    if(data->cur==0)
    {
        printk(KERN_WARNING "MemMap Flushin chunks\n");
        MemMap_FlushData(data);
        return 1;
    }
    return 0;
}


void *MemMap_AddrInChunkPos(task_data data,int pos, int chunkid)
{
    void *addr;
    if(!MEMMAP_VALID_CHUNKID(chunkid))
        return NULL;
    printk(KERN_WARNING "MemMap Looking for next addr in ch %d, pos %d/%u-%lu\n",
            chunkid, pos, data->chunks[chunkid]->nbentry, MEMMAP_TDATA_TABLE_SIZE);
    if(pos < 0 ||pos >= data->chunks[chunkid]->nbentry)
    {
        printk(KERN_WARNING "Nothing available\n");
        return NULL;
    }
    addr=data->chunks[chunkid]->table[pos].addr;
    printk(KERN_WARNING "found adress %p\n", addr);
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
    printk(KERN_WARNING "MemMap_FlushData not implemented yet\n");
    //TODO
    //print all chunks
    //Clean all chunks but last
}
