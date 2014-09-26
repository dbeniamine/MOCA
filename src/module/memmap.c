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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <memmap.h>
#include <memmap_probes.h>
#include <memmap_tlb.h>

/* Informations about the module */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Beniamine David.Beniamine@imag.fr");
MODULE_DESCRIPTION("MemMap's kernel module tracks memory access");

/* Parameters */

// PID of the main program to track
module_param(MemMap_mainPid, int, 0);
// Wakeup period in ms
module_param(MemMap_wakeupInterval,int,0);
// Maximum number of threads used by the monitored application
module_param(MemMap_numThreads,int,0);
// Priority for FIFO scheduler
module_param(MemMap_schedulerPriority,int,0);
static void MemMap_SetSchedulerPriority(void)
{
    printk(KERN_ALERT "MemMap setting its own priority to %d\n", 
            MemMap_SetSchedulerPriority);
    //TODO
    printk(KERN_ALERT "MemMap priority not implemented yet\n");
    return;
}

// Fuction called by insmod
static int __init MemMap_Init(void)
{
    printk(KERN_WARNING "MemMap started %d\n");
    // Set scheduler priority
    MemMap_SetSchedulerPriority();
    // Prepare data for Threads
    MemMap_InitThreads();
    printk(KERN_WARNING "MemMap correctly intialized \n");
    return 0;
}

// function called by rmmod
static void __exit MemMap_Exit(void)
{
    printk(KERN_WARNING "MemMap exiting\n");
    // Clean all memory used by threads structure
    MemMap_CleanThreads(void);
    printk(KERN_WARNING "MemMap exited\n");
}

// Panic exit function
void MemMap_Panic(const char *s)
{
    printk(KERN_ALERT "MemMap panic:\n%s\n", s);
    MemMap_Exit();
}

module_init(MemMap_Init);
module_exit(MemMap_Exit);
