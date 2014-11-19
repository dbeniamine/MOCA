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
#ifndef __MEMMAP__
#define __MEMMAP__
#define MAX(A,B) ((A)>(B) ? (A) : (B) )
#include <linux/module.h>
#include <linux/kernel.h>


// Panic exit function
void MemMap_Panic(const char *s);

#endif //__MEMMAP__

