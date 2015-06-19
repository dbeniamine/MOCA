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
#ifndef __MOCA__
#define __MOCA__

#ifdef MOCA_DEBUG
#define MOCA_DEBUG_PRINT(...) printk(KERN_DEBUG __VA_ARGS__)
#else
#define MOCA_DEBUG_PRINT(...)
#endif //MOCA_DEBUG
#define MAX(A,B) ((A)>(B) ? (A) : (B) )


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>


// Panic exit function
void Moca_Panic(const char *s);
//Clocks managment
void Moca_UpdateClock(void);
unsigned long Moca_GetClock(void);
int Moca_IsActivated(void);


#endif //__MOCA__

