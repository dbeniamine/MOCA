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
#ifndef __MEMMAP_TLB__
#define __MEMMAP_TLB__

#define MEMMAP_DEFAULT_WAKEUP_INTERVAL 200

/* Main function for TLB walkthrough
 * Check accessed page every Memmap_wakeupsIinterval ms
 */
int MemMap_MonitorTLBThread(void *);

#endif //__MEMMAP_TLB__
