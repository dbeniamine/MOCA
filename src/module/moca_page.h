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
#ifndef __MOCA_PAGE__
#define __MOCA_PAGE__


#define MOCA_DEFAULT_WAKEUP_INTERVAL 50

/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int Moca_MonitorThread(void *);

pte_t *Moca_PteFromAdress(unsigned long addr, struct mm_struct *mm);

/*
 * False pte fault hack:
 * We set PROTNONE bit and clear PAGE_PRESENT bit as if we were using mprotect
 * This hack should work for any program which doesn"t use mprotect
 */
#define MOCA_USE_FALSE_PTE
#ifdef MOCA_USE_FALSE_PTE
#include <asm/pgtable.h>
// The forbidden flag configuration
// Is pte in the "false" state ?
#define MOCA_PTE_FALSE(pte) ( (pte_val(*pte) & (_PAGE_PRESENT | _PAGE_PROTNONE) ) \
        == _PAGE_PROTNONE )
// Put pte in a clean state
#define MOCA_CLEAR_FALSE_PTE(pte) *(pte)=pte_set_flags(*(pte), _PAGE_PRESENT); \
        *(pte)=pte_clear_flags(*(pte), _PAGE_PROTNONE)
// Set pte to the "false state
#define MOCA_SET_FALSE_PTE(pte) *(pte)=pte_clear_flags(*(pte), _PAGE_PRESENT); \
        *(pte)=pte_set_flags(*(pte),_PAGE_PROTNONE)
#else
// Do not do the hack
#define MOCA_PTE_FALSE(pte) 0
#define MOCA_CLEAR_FALSE_PTE(pte)
#define MOCA_SET_FALSE_PTE(pte)
#endif



#endif //__MOCA_PAGE__
