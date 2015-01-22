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

void *Moca_PhyFromVirt(void *addr, struct mm_struct *mm);
#endif //__MOCA_PAGE__
