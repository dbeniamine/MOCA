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
#include "memmap.h"
#include "memmap_tlb.h"
#include "memmap_threads.h"
#include <linux/kthread.h>
#include <linux/smp.h> //get_cpu()
#include <linux/delay.h> //msleep

// Wakeup period in ms
int MemMap_wakeupInterval=MEMMAP_DEFAULT_WAKEUP_INTERVAL;

// Print out all recorded data
void flush_data(int id)
{
    //TODO
    printk(KERN_WARNING "Kthread %d flushing data \n", id);
}
//Walk through TLB and record all memory access
void MemMap_TLBWalk(int id)
{
    //TODO
    printk(KERN_WARNING "Kthread %d walking\n", id);
}
// Return 1 iff too many data are recorded
int MemMap_NeedToFlush(int id)
{
    //TODO
    return 0;
}

/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int MemMap_MonitorTLBThread(void * arg)
{
    //Init tlb walk data
    int myid=get_cpu();
    //Main loop
    while(!kthread_should_stop())
    {
        MemMap_TLBWalk(myid);
        if(MemMap_NeedToFlush(myid))
            flush_data(myid);
        msleep(MemMap_wakeupInterval);
    }
    flush_data(myid);
    return 0;
}

