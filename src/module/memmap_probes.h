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
#ifndef __MEMMAP_PROBES__
#define __MEMMAP_PROBES__

// Register all needed probes for the module
void MemMap_RegisterProbes(void);
// Unregister all probes
void MemMap_UnregisterProbes(void);

/* Clone syscall, see also clone2, clone3
 * used by pthread and fork
 *  if flags & CLONE_THREAd
 *      we are in a new thread
 *  else
 *      We are in a new process
*/
#endif //__MEMMAP_PROBES__
