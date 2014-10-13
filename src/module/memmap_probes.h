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

#ifndef __MEMMAP_PROBES__
#define __MEMMAP_PROBES__

int MemMap_RegisterProbes(void);
void MemMap_UnregisterProbes(void);

#endif //__MEMMAP_PROBES__
