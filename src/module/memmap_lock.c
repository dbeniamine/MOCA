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
#include "memmap_lock.h"
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>

typedef struct _memmap_lock
{
    int nbIn;
    int nbWait;
    spinlock_t lock_Glob;
    spinlock_t lock_Int;
}*MemMap_Lock_t;

MemMap_Lock_t MemMap_Lock_Init(void)
{
    MemMap_Lock_t lock;
    if(!(lock=kmalloc(sizeof(struct _memmap_lock),GFP_KERNEL)))
    {
        return NULL;
    }
    lock->nbIn=0;
    lock->nbWait=0;
    spin_lock_init(&lock->lock_Glob);
    spin_lock_init(&lock->lock_Int);
    return lock;
}

void MemMap_Lock_Wait(MemMap_Lock_t lock)
{
    //Here we must already held the internal lock
    /* spin_unlock(&lock->lock_Int); */
    msleep(10);
    /* spin_lock(&lock->lock_Int); */
}

//TODO: printk here
void MemMap_Lock(MemMap_Lock_t lock, int prio)
{
    /* spin_lock(&lock->lock_Int); */
    /* if(prio==MEMMAP_LOCK_PRIO_MAX) */
    /* { */
    /*     ++lock->nbWait; */
    /*     while(lock->nbIn) */
    /*         MemMap_Lock_Wait(lock); */
    /* } */
    /* else */
    /* { */
    /*     while(lock->nbWait || lock->nbIn) */
    /*         MemMap_Lock_Wait(lock); */
    /* } */
    spin_lock(&lock->lock_Glob);
    /* --lock->nbWait; */
    /* ++lock->nbIn; */
    /* spin_unlock(&lock->lock_Int); */
}
void MemMap_Unlock(MemMap_Lock_t lock)
{
    /* --lock->nbIn; */
    spin_unlock(&lock->lock_Glob);
}
