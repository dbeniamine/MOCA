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

/*
 * False pte fault hack:
 */
#define MOCA_USE_FALSE_PF
#ifdef MOCA_USE_FALSE_PF
#include <asm/pgtable.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
// The forbidden flag configuration
// Is pte in the "false" state ?
        /*&& !pte_file(ptent) ) \*/
#define MOCA_FALSE_PF(ptent,mm) ( !pte_present(ptent) ) && pte_file(ptent) \
        && !((mm)->mmap->vm_flags & VM_NONLINEAR )
/*        && non_swap_entry(pte_to_swp_entry(ptent)) \
        && !is_migration_entry(pte_to_swp_entry(ptent)) ) */
// Put pte in a clean state
#define MOCA_CLEAR_FALSE_PF(ptent) ptent=pte_set_flags(ptent, _PAGE_PRESENT);
// Set pte to the "false state
#define MOCA_SET_FALSE_PF(ptent) ptent=pte_clear_flags(ptent, _PAGE_PRESENT);
#else
// Do not do the hack
#define MOCA_FALSE_PF(ptent) 0
#define MOCA_CLEAR_FALSE_PF(ptent)
#define MOCA_SET_FALSE_PF(ptent)
#endif //MOCA_USE_FALSE_PF

#endif //__MOCA_FALSE_PF__
