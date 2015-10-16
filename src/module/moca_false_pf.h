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

#ifndef __MOCA_FALSE_PF__
#define __MOCA_FALSE_PF__
#define __NO_VERSION__

#include <linux/spinlock.h>
#include <asm/pgtable.h>

// Init data struct required for managing false pf hack
void Moca_InitFalsePf(void);
void Moca_ClearFalsePfData(void);

// Marks the nb addr as not present and save them as false page fault
// Requires write lock to be held
void Moca_AddFalsePf(struct mm_struct *mm, unsigned long addr);



/*
 * Try to fix false fault on addr.
 * Does nothing if addr isn't in the false addr list
 * returns 0 on success
 *         1 if addr is not in the false pf list
 *         1 if addr is not in the false pf list
 */
int Moca_FixFalsePf(struct mm_struct *mm, unsigned long addr,int cpu);

/*
 * Remove all false page faults associated to mm and set the present flags
 * back
 * Requires Read lock to be held
 */
void Moca_FixAllFalsePf(struct mm_struct *mm,int cpu);

void *Moca_PhyFromVirt(void *addr, struct mm_struct *mm);

void Moca_RLockPf(void);
void Moca_RUnlockPf(void);
void Moca_WLockPf(void);
void Moca_WUnlockPf(void);
#endif //__MOCA_FALSE_PF__

