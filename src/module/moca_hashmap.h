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
#ifndef __MOCA_HASHMAP_H__
#define __MOCA_HASHMAP_H__
#define __NO_VERSION__
#define MOCA_HASHMAP_ADDED 0
#define MOCA_HASHMAP_ERROR -1
#define MOCA_HASHMAP_FULL -2
#define MOCA_HASHMAP_ALREADY_IN_MAP  -3
#include "moca.h"

/*
 * This package provide very low memory cost hash map.
 * The hash function used is hash_ptr from linux/hash.
 * One can use these hashmaps with any structure that start with a void* used
 * as the key and an integer which must not be modified out of this package.
 * All allocations are done at the initialisation to speed up the insertion.
 */
typedef struct _hash_entry
{
    void *key;
    int next;
}*hash_entry;


typedef struct _hash_map *hash_map;

/* Initialize an empty map of (1<<hash_bits) different keys, able to hold
 * nb_elt elements
 * An entry of this map must be castable in hash_entry and have a size of
 * elt_size.
 */
hash_map Moca_InitHashMap(unsigned long hash_bits, int nb_elt, size_t elt_size);

// Return the number of entry in map
int Moca_NbElementInMap(hash_map map);

/*
 * Returns -1 if key is not in map
 *         the position of key in the map if it is present
 */
int Moca_PosInMap(hash_map map,void *key);

/*
 * Return the hash entry corresponding to key,
 *        NULL if key is not in the map
 */
hash_entry Moca_EntryFromKey(hash_map map, void *key);

/*
 * Insert key in map
 * Returns A pointer to the hash_entry corresponding to key
 *         Null in case of error
 * status is set to:
 *         The position of hash_entry in case of success
 *         One of the following in case of errors:
 *          MOCA_HASHMAP_ALREADY_IN_MAP
 *          MOCA_HASHMAP_FULL
 *          MOCA_HASHMAP_ERROR
 */
hash_entry Moca_AddToMap(hash_map map, void *key, int *status);
/*
 * Returns the hash entry at position pos
 *         Null if pos is invalid or there is no entry at this position
 */
hash_entry Moca_EntryAtPos(hash_map map, unsigned int pos);
/*
 * Returns the next entry starting from pos
 *  NULL if there is no entry after pos
 */
hash_entry Moca_NextEntryPos(hash_map map, unsigned int *pos);

// Remove key from the map
hash_entry Moca_RemoveFromMap(hash_map map,void *key);
// Reset the map, be carefull with this one
void Moca_ClearMap(hash_map map);
void Moca_FreeMap(hash_map map);
#endif // __MOCA_HASHMAP_H__
