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

#include "false_moca.h"
#include "atomic.h"
#include "moca.h"
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#define MAX_FILES 100
#define nbTh 100
#define nbRTh 20
#define POOL_SIZE PAGE_SIZE*20

int nbIterations=100000;
int FlusherShouldDie=0;
int MonitorShouldDie=0;
pthread_t MonitorTh;

struct file moca_files[MAX_FILES];
atomic_t nb_files=ATOMIC_INIT(1);
struct task_struct *tasks[nbTh];


pthread_t threads[nbTh];
pthread_t FlusherTh;



extern int Moca_MonitorThread(void * arg);

extern int Moca_HandlerPost(struct kretprobe_instance *ri,
        struct pt_regs *regs);

extern void Moca_MmFaultHandler(struct mm_struct *mm, struct vm_area_struct *vma,
        unsigned long address, unsigned int flags);


extern void Moca_ExitHandler(struct mmu_gather *tlb, struct vm_area_struct *start_vma,
        unsigned long start, unsigned long end);

void remove_proc_entry(const char *b,struct proc_dir_entry *p)
{
    int pos=1;
    int n=atomic_read(&nb_files);
    while(pos < MAX_FILES &&
            (moca_files[pos].status!=0 || strcmp(b,moca_files[pos].name)!=0))
        ++pos;
    pthread_mutex_lock(&moca_files[pos].lock);
    moca_files[pos].status=1;
    pthread_mutex_unlock(&moca_files[pos].lock);
}

struct proc_dir_entry *proc_create_data(const char *b, int n,struct proc_dir_entry* p,
        const struct file_operations *fops,void *data)
{
    int pos=atomic_inc_return(&nb_files)-1;
    moca_files[pos].data=data;
    moca_files[pos].fops=fops;
    moca_files[pos].status=0;
    moca_files[pos].name=malloc(strlen(b)+1);
    moca_files[pos].name=strcpy(moca_files[pos].name,b);
    pthread_mutex_init(&moca_files[pos].lock,NULL);
    return (struct proc_dir_entry *)pos;
}

int getid(void)
{
    pthread_t th=pthread_self();
    int i=0;
    while(i<nbTh && threads[i]!=th)
        ++i;
    return i;
}

struct task_struct *current(void)
{
    int id=getid();
    if(id < 0 || id > nbTh)
        return NULL;
    return tasks[id];
}

void init_mm(struct task_struct *t)
{
    t->mm=malloc(sizeof(struct mm_struct));
    t->mm->pgd=calloc(POOL_SIZE,sizeof(int));
    t->vma=malloc(sizeof(struct vm_area_struct));
    t->vma->vm_mm=t->mm;
}

struct task_struct *init_task(struct task_struct *parent)
{
    struct task_struct *t=malloc(sizeof(struct task_struct));
    t->real_parent=parent;
    t->on_cpu=0;
    t->sched_info.last_arrival=0;
    init_mm(t);
    return t;
}

void Init_mm_tasks(void)
{
    int i;
    // Create monitored task
    tasks[1]=init_task(NULL);
    tasks[0]=init_task(tasks[1]);
    for(i=2; i< nbRTh;++i)
        tasks[i]=init_task(tasks[i-1]);
    // Un monitored tasks
    tasks[i]=init_task(NULL);
    while(i<nbTh)
    {
        tasks[i]=init_task(tasks[i-1]);
        ++i;
    }
}

struct task_struct *pid_task(int pid, int type)
{
    if(pid >= 0 && pid < nbRTh)
        return tasks[pid];
    return NULL;
}

void do_exit(struct task_struct *t, int reinit)
{
    struct mm_struct *oldmm=t->mm;
    Moca_ExitHandler(NULL,t->vma,0UL,0UL);
    if(reinit)
    {
        init_mm(t);
    }
    else
    {
        t->mm=NULL;
        t->vma->vm_mm=NULL;
    }
    free(oldmm);
}

unsigned long pick_addr(struct mm_struct *mm)
{
    int delta=rand()%POOL_SIZE;
    return (unsigned long) mm->pgd+delta;
}

void do_pagefault(struct task_struct *t)
{
    int flags=0;
    if(rand()%1000 > 10)
        flags|=FAULT_FLAG_USER;
    if(rand()%1000 > 50)
        flags|=FAULT_FLAG_WRITE;
    Moca_MmFaultHandler(t->mm,t->vma,pick_addr(t->mm),flags);
}

void sleepifneeded(void)
{
    int r=rand()%10000;
    if( r< 10)
        msleep(1);
}

void *do_stuff(void * arg)
{
    int i,id=(int)arg;
    struct task_struct *self=tasks[id];
    ++self->sched_info.last_arrival;
    for(int i=0;i<nbIterations;++i)
    {
        /* printf("Thread %d starting work iteration %d\n",id,i); */
        if(rand()%100000 < 10)
            do_exit(self,1);
        else
            do_pagefault(self);
        usleep(10);
        Moca_HandlerPost(NULL,NULL);
        /* printf("Thread %d sleeping iteration %d\n",id,i); */
        sleepifneeded();
        /* printf("Thread %d end iteration %d\n",id,i); */
    }
    msleep(10);
    do_exit(self,0);
    Moca_HandlerPost(NULL,NULL);
    return NULL;
}

void *do_flush(void *arg)
{
    char *buff=malloc((1<<14)*sizeof(char));
    while(!FlusherShouldDie)
    {
        int n=atomic_read(&nb_files);
        /* printf("Flusher awake %d files\n",n); */
        for(int i=1;i<n;++i)
        {
            pthread_mutex_lock(&moca_files[i].lock);
            if(moca_files[i].status==0 && moca_files[i].data!=NULL)
                while(moca_files[i].fops->read(&moca_files[i],buff,1<<14,NULL)!=0);
            pthread_mutex_unlock(&moca_files[i].lock);
        }
        /* printf("Flusher sleeping\n"); */
        usleep(50000);
    }
    /* printf("Flusher dying\n"); */
    free(buff);
    return NULL;
}

extern int Moca_Init(void);
extern void Moca_Exit(void);

int main()
{
    int i;
    void *res;
    srand(time(NULL));
    printf("Init mm\n");
    //Init memory and "tasks"
    Init_mm_tasks();
    //Init data
    printf("Init atomics\n");
    init_atomic();
    // Call module
    printf("Init moca\n");
    Moca_Init();
    printf("Starting threads\n");
    pthread_create(&FlusherTh,NULL,do_flush,NULL);
    // Call threads
    for(i=0;i<nbTh;++i)
    {
        pthread_create(&threads[i],NULL,do_stuff,(void *)i);
    }
    // Wait for threads
    for(i=0;i<nbTh;++i)
    {
        printf("Waiting for thread %d\n",i);
        pthread_join(threads[i], &res);
    }
    Moca_Exit();
    FlusherShouldDie=1;
    printf("Waiting for thread flusher\n");
    pthread_join(FlusherTh, &res);
    printf("Simulator ended\n");
}
