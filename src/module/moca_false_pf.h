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
#define __NO_VERSION__
#ifndef __MOCA_FALSE_PF__
#define __MOCA_FALSE_PF__
//#include <linux/mm.h>
#include <asm/pgtable.h>
//#include <linux/swap.h>
//#include <linux/swapops.h>
//#include <linux/mm_types.h>

#define MOCA_USEFULL_PTE(pte) ( (pte) && !pte_none(*(pte))  )
        /* !pte_special(*(pte)) ) */

// Init data struct required for managing false pf hack
void Moca_InitFalsePf(void);

// Mark pte as not present, and save it as the false page fault
void Moca_AddFalsePf(struct mm_struct *mm, pte_t *pte);


/*
 * Try to fix false pte fault on pte.
 * Does nothing if pte isn't in the false pte list
 * returns 0 on success
 *         1 if pte is not in the false pf list
 */
int Moca_FixFalsePf(struct mm_struct *mm, pte_t *pte);

/*
 * Remove all false page faults associated to mm and set the present flags
 * back
 */
void Moca_FixAllFalsePf(struct mm_struct *mm);

#endif //__MOCA_FALSE_PF__

