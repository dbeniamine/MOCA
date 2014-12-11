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
//#define __NO_VERSION__
//#include <linux/slab.h>
//#include <linux/hash.h>
#include "memmap_hashmap.h"
#include "memmap.h"


#define tableElt(map, ind) \
    ( (hash_entry)((char *)((map)->table)+((ind)*((map)->elt_size)) ))

typedef struct _hash_map
{
    int *hashs;
    hash_entry *table;
    unsigned int nbentry;
    unsigned long hash_bits;
    unsigned long size;
    unsigned long tableSize;
    size_t elt_size;
}*hash_map;

hash_map MemMap_InitHashMap(unsigned long hash_bits, int factor,
        size_t elt_size)
{
    unsigned int i;
    hash_map map=kmalloc(sizeof(struct _hash_map), GFP_ATOMIC);
    if(!map)
        return NULL;
    map->hash_bits=hash_bits;
    map->size=1UL<<hash_bits;
    map->tableSize=factor*map->size;
    map->nbentry=0;
    map->elt_size=elt_size;
    MEMMAP_DEBUG_PRINT("MemmMap allocationg hash size %lu\n", map->size);
    if(!(map->hashs=kmalloc(sizeof(int)*map->size,GFP_ATOMIC)))
    {
        kfree(map);
        return NULL;
    }
    for(i=0;i<map->size;++i)
    {
        map->hashs[i]=-1;
    }
    if(!(map->table=kcalloc(map->tableSize,elt_size,GFP_ATOMIC)))
    {
        kfree(map->hashs);
        kfree(map);
        return NULL;
    }
    return map;
}

int MemMap_NbElementInMap(hash_map map)
{
    if(!map)
        return -1;
    return map->nbentry;
}

/*
 * Returns -1 if key is not in map
 *         the position of key in the map if it is present
 */
int MemMap_PosInMap(hash_map map,void *key)
{
    unsigned long h;
    int ind=0;
    if(!map)
        return -1;
    h=hash_ptr(key, map->hash_bits);
    ind=map->hashs[h];
    while(ind!= -1 && tableElt(map,ind)->key!=key )
        ind=tableElt(map,ind)->next;
    return ind;
}

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
hash_entry MemMap_AddToMap(hash_map map, void *key, int *status)
{
    unsigned long h;
    int ind=0;
    if(!map)
    {
        *status=MEMMAP_HASHMAP_ERROR;
        return NULL;
    }
    if(map->nbentry==map->tableSize)
    {
        *status=MEMMAP_HASHMAP_FULL;
        return NULL;
    }
    MEMMAP_DEBUG_PRINT("MemMap Inserting %p in map %p\n", key, map);
    h=hash_ptr(key, map->hash_bits);
    ind=map->hashs[h];
    if(ind==-1)
    {
        map->hashs[h]=map->nbentry;
    }
    else
    {
        while(tableElt(map,ind)->key!=key && tableElt(map,ind)->next!=-1)
            ind=tableElt(map,ind)->next;
        if(tableElt(map,ind)->key==key)
        {
            MEMMAP_DEBUG_PRINT("MemMap %p already in map %p\n", key, map);
            *status=MEMMAP_HASHMAP_ALREADY_IN_MAP;
            return tableElt(map,ind);
        }
        MEMMAP_DEBUG_PRINT("MemMap collision in map %p key %p\n", key, map);
        tableElt(map,ind)->next=map->nbentry;
    }

    MEMMAP_DEBUG_PRINT("MemMap inserting %p ind %d/%lu\n",
            key,map->nbentry,map->tableSize);
    ind=map->nbentry++;
    tableElt(map,ind)->key=key;
    tableElt(map,ind)->next=-1;;
    MEMMAP_DEBUG_PRINT("MemMap Inserted %p in map %p\n", key, map);
    *status=ind;
    return tableElt(map,ind);
}

/*
 * Returns the hash entry at position pos
 *         Null is pos is invalid or there is no entry at this position
 */
hash_entry MemMap_EntryAtPos(hash_map map, unsigned int pos)
{
    if(!map || pos >= map->nbentry)
        return NULL;
    return tableElt(map,pos);
}
hash_entry MemMap_RemoveFromMap(hash_map map,void *key)
{
    unsigned long h;
    int ind, ind_prev=-1;
    if(!map)
        return NULL;
    MEMMAP_DEBUG_PRINT("MemMap removing %p from %p\n", key, map);
    h=hash_ptr(key, map->hash_bits);
    ind=map->hashs[h];
    while(ind!= -1 && tableElt(map,ind)->key!=key )
    {
        ind_prev=ind;
        ind=tableElt(map,ind)->next;
    }
    MEMMAP_DEBUG_PRINT("MemMap removing %p from %p ind %d prev %d\n", key, map, ind, ind_prev);
    //key wasn't in map
    if(ind==-1 )
        return NULL;
    //Remove from list
    if(ind_prev!=-1)
    {
        tableElt(map,ind_prev)->next=tableElt(map,ind)->next;
    }
    else
    {
        map->hashs[h]=tableElt(map,ind)->next;
    }
    tableElt(map,ind)->next=-1;
    --map->nbentry;
    MEMMAP_DEBUG_PRINT("MemMap removing %p from %p ind %d ok\n", key, map, ind);
    return tableElt(map,ind);
}


// Clear map, after this call, map is still usable
void MemMap_ClearMap(hash_map map)
{
    unsigned int i;
    unsigned long h;
    if(!map)
        return;
    for(i=0;i<map->nbentry;++i)
    {
        h=hash_ptr(tableElt(map,i)->key, map->hash_bits);
        map->hashs[h]=-1;
        tableElt(map,i)->next=-1;
    }
}

void MemMap_FreeMap(hash_map map)
{
    if(!map)
        return;
    if(map->table)
        kfree(map->table);
    if(map->hashs)
        kfree(map->hashs);
    kfree(map);
}
