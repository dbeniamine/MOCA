/*
 * Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
 * Author: Beniamine, David <David@Beniamine.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//false moca
#ifndef __MOCA_H__
#define __MOCA_H__

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// printf

//#define DO_PRINT

#ifdef DO_PRINT
#define PRINT(...) printf(__VA_ARGS__)
#define MOCA_DEBUG_PRINT(...) printf(__VA_ARGS__)
#define printk(...) printf(__VA_ARGS__)
#else
#define PRINT(...)
#define MOCA_DEBUG_PRINT(...)
#define printk(...)
#endif


// Malloc
#define kmalloc(A,B) malloc(A)
#define kcalloc(A,B,C) calloc(A,B)
#define kfree(A) free(A)

#define hash_ptr(k, bits) hash_64((unsigned long)(k),(bits))
// This is an easy hack to test collisions
//#define hash_ptr(k, bits) 37

typedef unsigned long u64;

//from linux/hash.h
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

#define Moca_Panic(A) printf(A);

static __always_inline u64 hash_64(u64 val, unsigned int bits)
{
	u64 hash = val;

	/*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
	u64 n = hash;
	n <<= 18;
	hash -= n;
	n <<= 33;
	hash -= n;
	n <<= 3;
	hash += n;
	n <<= 3;
	hash -= n;
	n <<= 4;
	hash += n;
	n <<= 2;
	hash += n;

	/* High bits are more random, so use them. */
	return hash >> (64 - bits);
}
#endif
