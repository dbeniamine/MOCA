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

#ifndef __MOCA_PROBES__
#define __MOCA_PROBES__

int Moca_RegisterProbes(void);
void Moca_UnregisterProbes(void);

#endif //__MOCA_PROBES__
