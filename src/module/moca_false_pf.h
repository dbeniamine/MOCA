/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Moca is a kernel module designed to track memory access
 *
 * Copyright (C) 2010 David Beniamine
 * Author: David Beniamine <David.Beniamine@imag.fr>
 */
#ifndef __MOCA_FALSE_PF__
#define __MOCA_FALSE_PF__
#define __NO_VERSION__

#include <linux/spinlock.h>
#include <asm/pgtable.h>

// Init data struct required for managing false pf hack
void Moca_InitFalsePf(void);
// the two following routines requires write lock to be held
void Moca_ClearFalsePf(void);
void Moca_ClearFalsePfData(void);

/* Marks the nb addr as not present and save them as false page fault
 * Set young and dirty to the value of the young and dirty flags from the pte
 */
void Moca_AddFalsePf(struct mm_struct *mm, unsigned long addr,
        int *young, int * dirty);



/*
 * Try to fix false pte fault on addr.
 * Does nothing if addr isn't in the false pte list
 * returns 0 on success
 *         1 if addr is not in the false pf list
 * Requires Read lock to be held
 */
int Moca_FixFalsePf(struct mm_struct *mm, unsigned long addr);

/*
 * Remove all false page faults associated to mm and set the present flags
 * back
 * Requires Read lock to be held
 */
void Moca_FixAllFalsePf(struct mm_struct *mm);

void *Moca_PhyFromVirt(void *addr, struct mm_struct *mm);

void Moca_GetCountersFromAddr(unsigned long addr, struct mm_struct *mm,
        int * young, int *dirty);

/*
 * There is no need to use these locking function before calling any of the
 * subroutines above.
 * It is only usefull for the logging thread which need to be sure that a task
 * does not held any lock before pausing it.
 */
void Moca_WLockPf(void);
void Moca_WUnlockPf(void);
void Moca_RLockPf(void);
void Moca_RUnlockPf(void);

#endif //__MOCA_FALSE_PF__

