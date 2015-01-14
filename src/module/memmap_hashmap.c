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
//#define MEMMAP_DEBUG

#include <linux/slab.h>
#include <linux/hash.h>
#include "memmap_hashmap.h"


#define MEMMAP_HASHMAP_END -1
#define MEMMAP_HASHMAP_UNUSED -2
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

unsigned int MemMap_FindNextAvailPosMap(hash_map map)
{
    unsigned int i=0;
    while(i< map->tableSize && tableElt(map,i)->next!=MEMMAP_HASHMAP_UNUSED)
        ++i;
    return i;
}

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
        map->hashs[i]=MEMMAP_HASHMAP_END;
    if(!(map->table=kcalloc(map->tableSize,elt_size,GFP_ATOMIC)))
    {
        kfree(map->hashs);
        kfree(map);
        return NULL;
    }
    for(i=0;i<map->tableSize;++i)
        tableElt(map,i)->next=MEMMAP_HASHMAP_UNUSED;
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
    while(ind>=0 && tableElt(map,ind)->key!=key )
        ind=tableElt(map,ind)->next;
    return ind;
}

/*
 * Return the hash entry corresponding to key,
 *        NULL if key is not in the map
 */
hash_entry MemMap_EntryFromKey(hash_map map, void *key)
{
    unsigned long h;
    int ind=0;
    if(!map)
        return NULL;
    h=hash_ptr(key, map->hash_bits);
    ind=map->hashs[h];
    while(ind>=0 && tableElt(map,ind)->key!=key )
        ind=tableElt(map,ind)->next;
    if(ind >=0)
        return tableElt(map,ind);
    return NULL;
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
    int ind=0, nextPos;
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
    //Do the insertion
    nextPos=MemMap_FindNextAvailPosMap(map);
    MEMMAP_DEBUG_PRINT("MemMap inserting %p ind %d/%lu\n",
            key,nextPos,map->tableSize);
    if((unsigned)nextPos >= map->tableSize)
    {
        *status=MEMMAP_HASHMAP_ERROR;
        MemMap_Panic("BUG in findavailposmap");
        return NULL;
    }
    //Update the link
    h=hash_ptr(key, map->hash_bits);
    ind=map->hashs[h];
    if(ind<0)
    {
        tableElt(map,nextPos)->key=key;
        tableElt(map,nextPos)->next=MEMMAP_HASHMAP_END;
        map->hashs[h]=nextPos;
    }
    else
    {
        while(tableElt(map,ind)->key!=key && tableElt(map,ind)->next>=0)
            ind=tableElt(map,ind)->next;
        if(tableElt(map,ind)->key==key)
        {
            MEMMAP_DEBUG_PRINT("MemMap %p already in map %p\n", key, map);
            *status=MEMMAP_HASHMAP_ALREADY_IN_MAP;
            tableElt(map,nextPos)->key=NULL;
            return tableElt(map,ind);
        }
        MEMMAP_DEBUG_PRINT("MemMap collision in map %p key %p\n", key, map);
        tableElt(map,nextPos)->key=key;
        tableElt(map,nextPos)->next=MEMMAP_HASHMAP_END;
        tableElt(map,ind)->next=nextPos;
    }
    ++map->nbentry;
    MEMMAP_DEBUG_PRINT("MemMap Inserted %p in map %p\n", key, map);
    *status=nextPos;
    return tableElt(map,nextPos);
}

/*
 * Returns the hash entry at position pos
 *         Null is pos is invalid or there is no entry at this position
 */
hash_entry MemMap_EntryAtPos(hash_map map, unsigned int pos)
{
    if(!map || pos >= map->tableSize ||
            tableElt(map,pos)->next==MEMMAP_HASHMAP_UNUSED)
        return NULL;
    return tableElt(map,pos);
}
/*
 * Find the first available entry from pos
 * Returns the entry on success
 *         NULL if there is no more entry
 * After the call to Memmap_NextEntryPos, pos is the position of the returned
 * entry +1
 * This function can be used as an iterator
 */
hash_entry MemMap_NextEntryPos(hash_map map, unsigned int *pos)
{
    unsigned int i=*pos;
    MEMMAP_DEBUG_PRINT("Searching element after %d /%d\n",
            i, MemMap_NbElementInMap(map));
    while(i< map->tableSize && tableElt(map,i)->next==MEMMAP_HASHMAP_UNUSED)
        ++i;
    *pos=i+1;
    MEMMAP_DEBUG_PRINT("found  %p at %d\n",
            i>=map->tableSize?NULL:tableElt(map,i),i);
    if(i >=map->tableSize)
        return NULL;
    return tableElt(map,i);
}


hash_entry MemMap_RemoveFromMap(hash_map map,void *key)
{
    unsigned long h;
    int ind, ind_prev=MEMMAP_HASHMAP_END;
    if(!map)
        return NULL;
    MEMMAP_DEBUG_PRINT("MemMap removing %p from %p\n", key, map);
    h=hash_ptr(key, map->hash_bits);
    ind=map->hashs[h];
    while(ind>=0 && tableElt(map,ind)->key!=key )
    {
        ind_prev=ind;
        ind=tableElt(map,ind)->next;
    }
    MEMMAP_DEBUG_PRINT("MemMap removing %p from %p ind %d prev %d\n", key, map, ind, ind_prev);
    //key wasn't in map
    if(ind<0 )
        return NULL;
    //Remove from list
    if(ind_prev>=0)
    {
        tableElt(map,ind_prev)->next=tableElt(map,ind)->next;
    }
    else
    {
        map->hashs[h]=tableElt(map,ind)->next;
    }
    tableElt(map,ind)->next=MEMMAP_HASHMAP_UNUSED;
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
        map->hashs[h]=MEMMAP_HASHMAP_END;
        tableElt(map,i)->next=MEMMAP_HASHMAP_UNUSED;
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
