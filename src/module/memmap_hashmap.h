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
#ifndef __MEMMAP_HASHMAP_H__
#define __MEMMAP_HASHMAP_H__
#define __NO_VERSION__
#define MEMMAP_HASHMAP_ADDED 0
#define MEMMAP_HASHMAP_ERROR -1
#define MEMMAP_HASHMAP_FULL -2
#define MEMMAP_HASHMAP_ALREADY_IN_MAP  -3

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

/* Initialize an empty map of (1<<hash_bit) different keys, (1<<hash_bit)
 * elements max.
 * An entry of this map must be castable in hash_entry and have a size of
 * elt_size.
 */
hash_map MemMap_InitHashMap(unsigned long hash_bits, int factor,
        size_t elt_size);

// Return the number of entry in map
int MemMap_NbElementInMap(hash_map map);

/*
 * Returns -1 if key is not in map
 *         the position of key in the map if it is present
 */
int MemMap_PosInMap(hash_map map,void *key);

/*
 * Insert key in map
 * Returns A pointer to the hash_entry corresponding to key
 *         Null in case of error
 * status is set to:
 *         The position of hash_entry in case of success
 *         One of the following in case of errors:
 *          MEMMAP_HASHMAP_ALREADY_IN_MAP
 *          MEMMAP_HASHMAP_FULL
 *          MEMMAP_HASHMAP_ERROR
 */
hash_entry MemMap_AddToMap(hash_map map, void *key, int *status);
/*
 * Returns the hash entry at position pos
 *         Null is pos is invalid or there is no entry at this position
 */
hash_entry MemMap_EntryAtPos(hash_map map, unsigned int pos);

// Remove key from the map
hash_entry MemMap_RemoveFromMap(hash_map map,void *key);
// Reset the map, be carefull with this one
void MemMap_ClearMap(hash_map map);
void MemMap_FreeMap(hash_map map);
#endif // __MEMMAP_HASHMAP_H__
