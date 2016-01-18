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
/* #define MOCA_DEBUG */

#define MOCA_DATA_STATUS_NORMAL 0
//Data won't be written anymore, but need to be saved to file
#define MOCA_DATA_STATUS_NEEDFLUSH 1
//Data have been outputed  but FlushData must be called again for EOF
#define MOCA_DATA_STATUS_DYING -1
//We can free data (after removing the /proc entry)
#define MOCA_DATA_STATUS_ZOMBIE -2
#define MOCA_DATA_STATUS_DYING_OR_ZOMBIE(data) ((data)->status < 0)
#define MOCA_HASH_BITS 15

#define MOCA_CHUNK_NORMAL 0
#define MOCA_CHUNK_ENDING 1
#define MOCA_CHUNK_USED 2

int Moca_taskDataHashBits=MOCA_HASH_BITS;
int Moca_taskDataChunkSize=1<<(MOCA_HASH_BITS+1);
int Moca_nbChunks=40;

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/cpumask.h> //num_online_cpus
#include <asm/uaccess.h>  /* for copy_*_user */
#include <linux/delay.h>
#include "moca.h"
#include "moca_taskdata.h"
#include "moca_tasks.h"
#include "moca_hashmap.h"
#include "moca_false_pf.h"
#include <asm/pgtable.h>


#define MOCA_PROCDIR_NAME "Moca"
#define MOCA_PROC_TRACE "full-trace"

static struct proc_dir_entry *Moca_proc_root=NULL;

static ssize_t Moca_FlushData(struct file *filp,  char *buffer,
        size_t length, loff_t * offset);

static const struct file_operations Moca_taskdata_fops = {
    .owner   = THIS_MODULE,
    .read    = Moca_FlushData,
};

typedef struct _chunk_entry
{
    void *key; //Virtual Address
    int next;
    int countR;
    int countW;
    int cpu;
}*chunk_entry;

typedef struct
{
    hash_map map;
    long startClock;
    long endClock;
    int cpu;
    int used;
    spinlock_t lock;
}chunk;

typedef struct _task_data
{
    struct task_struct *task;
    chunk **chunks;
    int cur;
    int currentlyFlushed;
    int currentlyFlushedPos;
    int internalId;
    int status;
    spinlock_t lock;
}*task_data;

static atomic_t Moca_nextTaskId=ATOMIC_INIT(0);

int Moca_CurrentChunk(task_data data)
{
    int ret=-1;
    spin_lock(&data->lock);
    ret=data->cur;
    spin_unlock(&data->lock);
    return ret;
}

int Moca_InitTaskData(void)
{
    //Procfs init
    Moca_proc_root=proc_mkdir(MOCA_PROCDIR_NAME, NULL);
    if(!Moca_proc_root)
        return 1;
    if(!proc_create_data(MOCA_PROC_TRACE,0444,Moca_proc_root,
                &Moca_taskdata_fops,NULL))
    {
        remove_proc_entry(MOCA_PROCDIR_NAME, NULL);
        return 1;
    }
    return 0;
}

void Moca_ChunkEntryInitializer(void *e)
{
    chunk_entry ce=(chunk_entry)e;
    ce->countR=0;
    ce->countW=0;
    ce->cpu=0;
}

task_data Moca_InitData(struct task_struct *t)
{
    int i;
    task_data data;
    //We must not wait here !
    data=kmalloc(sizeof(struct _task_data),GFP_ATOMIC);
    MOCA_DEBUG_PRINT("Moca Initialising data for task %p\n",t);
    if(!data)
        goto fail;
    data->chunks=kcalloc(Moca_nbChunks,sizeof(chunk),GFP_ATOMIC);
    if(!data->chunks)
    {
        goto clean;
    }
    for(i=0;i<Moca_nbChunks;i++)
    {
        MOCA_DEBUG_PRINT("Moca Initialising data chunk %d for task %p\n",i, t);
        data->chunks[i]=kcalloc(1,sizeof(chunk),GFP_ATOMIC);
        if(!data->chunks[i])
            goto cleanChunks;
        data->chunks[i]->cpu=0;
        data->chunks[i]->startClock=-1;
        data->chunks[i]->used=MOCA_CHUNK_NORMAL;
        data->chunks[i]->map=Moca_InitHashMap(Moca_taskDataHashBits,
                Moca_taskDataChunkSize, sizeof(struct _chunk_entry), NULL,
                Moca_ChunkEntryInitializer);
        if(!data->chunks[i]->map)
            goto cleanChunks;
        spin_lock_init(&data->chunks[i]->lock);
    }
    data->task=t;
    data->cur=0;
    data->internalId=atomic_inc_return(&Moca_nextTaskId)-1;
    data->status=MOCA_DATA_STATUS_NORMAL;
    data->currentlyFlushed=-1;
    data->currentlyFlushedPos=-1;
    spin_lock_init(&data->lock);
    return data;

cleanChunks:
    i=0;
    while(i< Moca_nbChunks && data->chunks[i]!=NULL)
    {
        if(data->chunks[i]->map!=NULL)
            Moca_FreeMap(data->chunks[i]->map);
        kfree(data->chunks[i]);
        ++i;
    }
clean:
    kfree(data);
fail:
    printk("Moca fail initializing data for task %p\n",t);
    return NULL;

}

void Moca_ClearAllData(void)
{
    int i=0, chunkid;
    moca_task t;
    MOCA_DEBUG_PRINT("Moca Cleaning data\n");
    while((t=Moca_NextTask(&i)))
    {
        MOCA_DEBUG_PRINT("Moca asking data %d %p %p to end\n",i, t->data, t->key);
        spin_lock(&t->data->lock);
        t->data->status=MOCA_DATA_STATUS_NEEDFLUSH;
        spin_unlock(&t->data->lock);
    }
    MOCA_DEBUG_PRINT("Moca flushing\n");
    i=0;
    while((t=Moca_NextTask(&i)))
    {
        //Wait for the task to be dead
        MOCA_DEBUG_PRINT("Moca waiting data %p : %d to end\n",t->data,i);
        while(!MOCA_DATA_STATUS_DYING_OR_ZOMBIE(t->data))
            msleep(100);
    }
    MOCA_DEBUG_PRINT("Moca Removing proc root\n");
    remove_proc_entry(MOCA_PROC_TRACE, Moca_proc_root);
    remove_proc_entry(MOCA_PROCDIR_NAME, NULL);
    i=0;
    //Clean must be done after removing the proc entry
    while((t=Moca_NextTask(&i)))
    {
        MOCA_DEBUG_PRINT("Moca data %d %p %p ended\n",i, t->data, t->key);
        for(chunkid=0; chunkid < Moca_nbChunks;++chunkid)
        {
            MOCA_DEBUG_PRINT("Memap Freeing data %p chunk %d\n",
                    t->data, chunkid);
            Moca_FreeMap(t->data->chunks[chunkid]->map);
            kfree(t->data->chunks[chunkid]);
        }
        kfree(t->data->chunks);
        kfree(t->data);
        Moca_RemoveTask(t->key);
        MOCA_DEBUG_PRINT("Moca Freed data %d \n", i);
    }
    MOCA_DEBUG_PRINT("Moca all data cleaned\n");
}



struct task_struct *Moca_GetTaskFromData(task_data data)
{
    if(!data)
        return NULL;
    return data->task;
}

int Moca_AddToChunk(task_data data, void *addr, int cpu, int write)
{
    int status, cur;
    struct _chunk_entry tmp;
    chunk_entry e;
    cur=Moca_CurrentChunk(data);
    spin_lock(&data->chunks[cur]->lock);
    if(data->chunks[cur]->used!=MOCA_CHUNK_NORMAL)
    {
        spin_unlock(&data->chunks[cur]->lock);
        return -1;
    }
    MOCA_DEBUG_PRINT("Moca hashmap adding %p to chunk %d %p data %p cpu %d\n",
            addr, cur, data->chunks[cur],data, cpu);
    tmp.key=addr;
    e=(chunk_entry)Moca_AddToMap(data->chunks[cur]->map,(hash_entry)&tmp, &status);
    switch(status)
    {
        case MOCA_HASHMAP_FULL :
            spin_unlock(&data->chunks[cur]->lock);
            printk(KERN_INFO "Moca chunk full, part of the trace will be lost");
            return -1;
            break;
        case MOCA_HASHMAP_ERROR :
            spin_unlock(&data->chunks[cur]->lock);
            printk("Moca hashmap error");
            return -1;
            break;
        case MOCA_HASHMAP_ALREADY_IN_MAP :
            MOCA_DEBUG_PRINT("Moca addr already in chunk %p\n", addr);
            ++e->countR;
            e->countW+=write;
            break;
        default :
            //Normal add
            e->countR=1;
            e->countW=write;
            break;
    }
    e->cpu|=1<<cpu;
    data->chunks[cur]->cpu|=1<<cpu;
    data->chunks[cur]->endClock=Moca_GetClock();
    if(data->chunks[cur]->startClock==-1)
        data->chunks[cur]->startClock=data->chunks[cur]->endClock;
    spin_unlock(&data->chunks[cur]->lock);
    return 0;
}

int Moca_NextChunks(task_data data)
{
    int cur,old;
    MOCA_DEBUG_PRINT("Moca Goto next chunks %p, %d\n", data, data->cur);
    //Global lock
    cur=old=Moca_CurrentChunk(data);
    spin_lock(&data->chunks[cur]->lock);
    data->chunks[cur]->used=MOCA_CHUNK_ENDING;
    do{
        MOCA_DEBUG_PRINT("Moca data %p : %d chunk %d status %d\n",data, data->internalId,
                cur,data->chunks[cur]->used);
        spin_unlock(&data->chunks[cur]->lock);
        cur=(cur+1)%Moca_nbChunks;
        spin_lock(&data->chunks[cur]->lock);
    }while(data->chunks[cur]->used!=MOCA_CHUNK_NORMAL && cur != old);
    spin_unlock(&data->chunks[cur]->lock);
    spin_lock(&data->lock);
    data->cur=cur;
    spin_unlock(&data->lock);
    MOCA_DEBUG_PRINT("Moca Goto chunks  %p %d, %d\n", data, cur,
            Moca_nbChunks);
    if(data->chunks[cur]->used!=MOCA_CHUNK_NORMAL)
    {
        printk(KERN_ALERT "Moca no more chunks, stopping trace for task %d\n You can fix that by relaunching Moca either with a higher number of chunks\n or by decreasing the logging daemon wakeupinterval\n",
                data->internalId);
    }
    return old;
}

void Moca_EndChunk(task_data data, int id)
{
    spin_lock(&data->chunks[id]->lock);
    data->chunks[id]->used=MOCA_CHUNK_USED;
    spin_unlock(&data->chunks[id]->lock);
}



void *Moca_AddrInChunkPos(task_data data,int *pos, int ch)
{
    chunk_entry e;
    if(ch < 0 || ch > Moca_nbChunks)
        return NULL;
    spin_lock(&data->chunks[ch]->lock);
    if(data->chunks[ch]->used==MOCA_CHUNK_USED)
    {
        spin_unlock(&data->chunks[ch]->lock);
        return NULL;
    }
    MOCA_DEBUG_PRINT("Moca Looking for next addr in ch %d, pos %d/%d\n",
            ch, *pos, Moca_NbElementInMap(data->chunks[ch]->map));
    e=(chunk_entry)Moca_NextEntryPos(data->chunks[ch]->map,pos);
    spin_unlock(&data->chunks[ch]->lock);
    if(!e)
        return NULL;
    MOCA_DEBUG_PRINT("Moca found adress %p at pos %d\n", e->key, *pos-1);
    return e->key;
}


int Moca_CpuMask(int cpu, char *buf, size_t size)
{
    int i=0,pos=num_online_cpus();
    if(!buf)
        return 0;
    while(pos>=0 && i< size)
    {
        buf[i]=(cpu&(1<<pos))?'1':'0';
        ++i;
        --pos;
    }
    if( i>=size)
    {
        printk("Moca Buffer overflow in CpuMask");
        return 0;
    }
    return i;
}




static moca_task Moca_currentlyFlushedTask=NULL;
static int Moca_currentlyFlushedTaskid=0;

#define LINE_WIDTH 512 // Way more then what we need
#define PROC_BUF_SIZE 131072 // Usual proc buf size
static char FLUSH_BUF[PROC_BUF_SIZE];
static ssize_t Moca_FlushData(struct file *filp,  char *buffer,
        size_t length, loff_t * offp)
{
    int chunkid, ind;
    static int csvinit=0;
    size_t sz=0,len=MIN(PROC_BUF_SIZE,length);
    chunk_entry e;
    task_data data=NULL;

    if(!Moca_currentlyFlushedTask ||
            Moca_currentlyFlushedTask->data->currentlyFlushed==-1)
    {
        // Not in the middle of a flush, get the next task
        if(!(Moca_currentlyFlushedTask=Moca_NextTask(&Moca_currentlyFlushedTaskid)))
            goto end; // No more task

        if(csvinit == 0)
        {
            // First flush ever
            sz+=snprintf(FLUSH_BUF+sz,len-sz,"@Virt, @Phy, Nreads, Nwrites, CPUMask, Start, End, TaskId\n");
            csvinit=1;
        }
    }

    do{
        data=Moca_currentlyFlushedTask->data;

        MOCA_DEBUG_PRINT("Moca_Flushing data %p id %d allowed len %lu\n", data,
                Moca_currentlyFlushedTaskid, len);

        if(MOCA_DATA_STATUS_DYING_OR_ZOMBIE(data))
        {
            //Data already flush, do noting and wait for kfreedom
            MOCA_DEBUG_PRINT("Moca dying data aborting flush%p\n",data);
            data->status=MOCA_DATA_STATUS_ZOMBIE;
            continue;
        }

        //Iterate on all chunks, start where we stopped if needed
        for(chunkid=(data->currentlyFlushed<0)?0:data->currentlyFlushed;
                chunkid < Moca_nbChunks; ++chunkid)
        {
            //If we are resuming a Flush, we are already helding the lock
            ind=(chunkid==data->currentlyFlushed)?data->currentlyFlushedPos:0;
            spin_lock(&data->chunks[chunkid]->lock);
            MOCA_DEBUG_PRINT("Moca flushing data %p : %d chunk %d  chz %d\n",
                    data, data->internalId, chunkid,
                    Moca_NbElementInMap(data->chunks[chunkid]->map));
            // Flush if needed
            if(data->status==MOCA_DATA_STATUS_NEEDFLUSH
                        || data->chunks[chunkid]->used==MOCA_CHUNK_USED)
            {
                while((e=(chunk_entry)Moca_NextEntryPos(
                                data->chunks[chunkid]->map,&ind)))
                {
                    if(LINE_WIDTH >= len-sz)
                    {
                        spin_unlock(&data->chunks[chunkid]->lock);
                        spin_lock(&data->lock);
                        data->currentlyFlushed=chunkid;
                        data->currentlyFlushedPos=ind;
                        spin_unlock(&data->lock);
                        MOCA_DEBUG_PRINT("Moca incomplete flush size %lu for data %p : %d chunk %d\n", sz,
                                data, data->internalId, chunkid);
                        goto out;
                    }
                    //@Virt @Phy countread countwrite cpumask start end
                    //taskid
                    sz+=snprintf(FLUSH_BUF+sz,len-sz, "%p, %p, %d, %d, ",
                            e->key,
                            Moca_PhyFromVirt(e->key, data->task->mm),
                            e->countR, e->countW);
                    sz+=Moca_CpuMask(e->cpu,FLUSH_BUF+sz,len-sz);
                    sz+=snprintf(FLUSH_BUF+sz,len-sz,", %lu, %lu, %d\n",
                            data->chunks[chunkid]->startClock,
                            data->chunks[chunkid]->endClock,
                            data->internalId);
                    //Re init data
                    e->countR=0;
                    e->countW=0;
                    e->cpu=0;
                }
                Moca_ClearMap(data->chunks[chunkid]->map);
                data->chunks[chunkid]->cpu=0;
                data->chunks[chunkid]->startClock=-1;
                data->chunks[chunkid]->used=MOCA_CHUNK_NORMAL;
                MOCA_DEBUG_PRINT("Moca flush data %p : %d chunk %d status %d\n", data,
                        data->internalId,chunkid,data->chunks[chunkid]->used);
            }
            spin_unlock(&data->chunks[chunkid]->lock);
        }
        spin_lock(&data->lock);
        data->currentlyFlushed=-1;
        data->currentlyFlushedPos=-1;
        if(data->status==MOCA_DATA_STATUS_NEEDFLUSH )
            data->status=MOCA_DATA_STATUS_DYING;
        MOCA_DEBUG_PRINT("Moca Complete Flush data %p : %d st: %d sz %lu\n",
                data,data->internalId,data->status, sz);
        spin_unlock(&data->lock);

    }while((Moca_currentlyFlushedTask=Moca_NextTask(
                    &Moca_currentlyFlushedTaskid)));
    // Restart the flush
end:
    MOCA_DEBUG_PRINT("Moca finished flush of all task\n");
    Moca_currentlyFlushedTaskid=0;

out:
    MOCA_DEBUG_PRINT("Moca complete flush size %lu\n", sz);
    sz-=copy_to_user(buffer,FLUSH_BUF,sz);
    return sz;
}
