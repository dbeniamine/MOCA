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

/*
 * Hashmap comparator
 * returns >0 if e1 > e2
 *         0  if e1==e2
 *         <0 else
 */
typedef int (*comp_fct_t)(hash_entry e1, hash_entry e2);

/* Initialize an empty map of (1<<hash_bits) different keys, able to hold
 * nb_elt elements
 * An entry of this map must be castable in hash_entry and have a size of
 * elt_size.
 * A comparator function can be given via the comp parameter. If non are
 * given, we use the default comparator function (key comparison).
 */
hash_map Moca_InitHashMap(unsigned long hash_bits, int nb_elt, size_t elt_size,
        comp_fct_t comp);

// Return the number of entry in map
int Moca_NbElementInMap(hash_map map);

/*
 * Returns -1 if e is not in map
 *         the position of e in the map if it is present
 */
int Moca_PosInMap(hash_map map,hash_entry e);

/*
 * Return the hash entry corresponding to e,
 *        NULL if e is not in the map
 */
hash_entry Moca_EntryFromKey(hash_map map, hash_entry e);

/*
 * Insert e (by copy) in map.
 * Returns A pointer to the hash_entry corresponding to e
 *         Null in case of error
 * status is set to:
 *         The position of hash_entry in case of success
 *         One of the following in case of errors:
 *          MOCA_HASHMAP_ALREADY_IN_MAP
 *          MOCA_HASHMAP_FULL
 *          MOCA_HASHMAP_ERROR
 */
hash_entry Moca_AddToMap(hash_map map, hash_entry e, int *status);
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

// Remove e from the map
hash_entry Moca_RemoveFromMap(hash_map map,hash_entry e);
// Reset the map, be carefull with this one
void Moca_ClearMap(hash_map map);
void Moca_FreeMap(hash_map map);
#endif // __MOCA_HASHMAP_H__
