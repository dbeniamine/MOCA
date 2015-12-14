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

//false moca
#ifndef __FALSE_MOCA_H__
#define __FALSE_MOCA_H__
#include <pthread.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sched.h>

#include "atomic.h"
#include "locks.h"
#define noop (void)0


// Stuff for taskdata
#define num_online_cpus() 4

#define proc_mkdir(A,B) proc_create_data(A,0,NULL,NULL,NULL)

#define THIS_MODULE 0

static inline void msleep(int s)
{
    usleep((unsigned int)(1000*(s)));
}

struct file{
    void *data;
    int status;
    struct file_operations *fops;
    const char *name;
    pthread_mutex_t lock;
};

struct file_operations{
    int owner;
    ssize_t (*read)(struct file *,char *,size_t,loff_t*);
};

struct proc_dir_entry *proc_create_data(const char *, int,struct proc_dir_entry*,
    const struct file_operations *,void *);

void remove_proc_entry(const char *,struct proc_dir_entry *);


#define file_inode(A) A
#define PDE_DATA(A) (A)->data

// KERNEL VERSION
#define LINUX_VERSION_CODE 3
#define KERNEL_VERSION(A,B,C) 3
#define task_pid_nr(t) 0

// Stuff for page faults
#define PAGE_SIZE (1UL<<12)
#define PAGE_MASK (~((PAGE_SIZE)-1))
#define TASK_SIZE ((1UL << 47) - PAGE_SIZE)
#define PTRS_PER_PGD (1<<12) // bad
#define PTRS_PER_PUD (1<<12) // bad
#define PTRS_PER_PMD (1<<12) // bad
#define PTRS_PER_PTE (1<<12) // bad
#define FAULT_FLAG_USER 0x10
#define FAULT_FLAG_WRITE 0x100
#define NR_CPUS 4
#define PIDTYPE_PID 0
#define find_vpid(A) 1

#define pte_none(A) !A
#define pte_special(A) !A
#define pmd_none(A) !A
#define pmd_bad(A) !A
#define pud_none(A) !A
#define pud_bad(A) !A
#define pgd_none(A) !A
#define pgd_bad(A) !A
#define pgd_present(A) A
#define pmd_present(A) A
#define pud_present(A) A
#define pte_present(A) A
#define pgd_offset(mm,a) (mm)->pgd
#define pmd_offset(p,a) (p)
#define pud_offset(p,a) (p)
#define pte_offset_map(p,a) (a)
#define pte_lockptr(mm,pmd) NULL
#define pte_unmap(pte) noop
#define pte_unmap_unlock(pte,ptl) noop
#define pte_clear_flags(pte,flags) *(int *)pte=0
#define pte_set_flags(pte,flags) *(int *)pte=1

#define __pa(a) (a)
#define pte_page(a) (a)

#define pid_alive(t) 1

struct task_struct *current(void);
struct task_struct * pid_task(int pid, int type);
typedef void *pgd_t ;
typedef void *pmd_t ;
typedef void *pud_t ;
typedef void *pte_t ;


struct vm_area_struct{
    struct mm_struct *vm_mm;
};
struct sched_str{
    long last_arrival;
};
struct task_struct{
    void *real_parent;
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    struct sched_str sched_info;
    int on_cpu;
};

struct mm_struct{
    pgd_t pgd;
};

struct kprobe{
    const char * symbol_name;
};
struct jprobe{
    struct kprobe kp;
    void *entry;
};
struct kretprobe{
    void *handler;
    int maxactive;
    struct kprobe kp;
};


//Unused struct just to make gcc happy
struct pt_regs {int r;};
struct kretprobe_instance {int r;};
struct mmu_gather {int r;};

#define register_kprobe(p) 0
#define register_kretprobe(p) 0
#define register_jprobe(p) 0
#define unregister_kprobe(p) 0
#define unregister_kretprobe(p) 0
#define unregister_jprobe(p) 0

#define jprobe_return() return

// printf

#define printk(...) printf(__VA_ARGS__)

#define dump_stack() noop
#define rcu_read_lock(l) noop
#define rcu_read_unlock(l) noop

// Malloc
#define kmalloc(A,B) malloc(A)
#define kcalloc(A,B,C) calloc(A,B)
#define kfree(A) free(A)

#define MODULE_LICENSE(A)
#define MODULE_AUTHOR(A)
#define MODULE_DESCRIPTION(A)
#define module_param(A,b,c)
#define module_init(A)
#define module_exit(A)
#define get_task_struct(A) noop
#define put_task_struct(A) noop
#define wake_up_process(A) noop
#define __init
#define __exit
#define KERN_NOTICE
#define KERN_INFO
#define KERN_ALERT
#define KERN_DEBUG


// Threads
extern int MonitorShouldDie;
extern pthread_t MonitorTh;

static inline int kthread_stop(struct task_struct *t)
{
    int *ret;
    MonitorShouldDie=1;
    pthread_join(MonitorTh, (void **)&ret);
    return (int)ret;
}
#define kthread_should_stop(A) (MonitorShouldDie==1)

static inline struct task_struct *kthread_create(int (*fct)(void*), void *foo, const char *bar)
{
    (void)foo;
    (void)bar;
    pthread_create(&MonitorTh,NULL,(void *(*)(void*))fct,NULL);
    return (struct task_struct *)&MonitorTh;
}

#define get_cpu() sched_getcpu()

// Hashs
#define hash_ptr(k, bits) hash_64((unsigned long)(k),(bits))

// This is an easy hack to test collisions
//#define hash_ptr(k, bits) 37

typedef unsigned long u64;

//from linux/hash.h
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

static __always_inline u64 hash_64(u64 val, unsigned int bits)
{
	u64 hash = val;

	/*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
	u64 n = hash;
	n <<= 18;
	hash -= n;
	n <<= 33;
	hash -= n;
	n <<= 3;
	hash += n;
	n <<= 3;
	hash -= n;
	n <<= 4;
	hash += n;
	n <<= 2;
	hash += n;

	/* High bits are more random, so use them. */
	return hash >> (64 - bits);
}

#endif
