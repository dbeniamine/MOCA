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
