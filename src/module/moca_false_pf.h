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

#define MOCA_USEFULL_PTE(pte) ((pte) && !pte_none(*(pte)) ) /*\
        && !pte_special(*(pte)))*/

/*
 * False pte fault hack:
 */
#define MOCA_USE_FALSE_PF
#ifdef MOCA_USE_FALSE_PF
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/mm_types.h>
// The forbidden flag configuration
// Is pte in the "false" state ?
        /*&& !pte_file(ptent) ) \*/
#define MOCA_FALSE_PF(ptent) ( !pte_present(ptent) && !is_swap_pte(ptent) )
/*
( !pte_none(ptent) && !pte_present(ptent) && \
        !!pte_special(*pte) && !(pte_flags(ptent) & _PAGE_PROTNONE ) ) && \
        !is_swap_pte(ptent))*/
 /*&& pte_file(ptent) \
        && ( !((mm)->mmap->vm_flags & VM_NONLINEAR) \
                || !lookup_swap_cache(pte_to_swp_entry(ptent),0) )
        && non_swap_entry(pte_to_swp_entry(ptent)) \
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

#define MOCA_PRINT_FLAGS(pte) MOCA_DEBUG_PRINT("Moca pte %p, none %d, \
        present %u, protnone %u,special %u, all %x\n" ,\
        pte, pte_none(*pte), pte_present(*pte), \
        (unsigned int)(pte_flags(*pte)&_PAGE_PROTNONE), pte_special(*pte),\
        (unsigned int)pte_flags(*pte));
#endif //__MOCA_FALSE_PF__

