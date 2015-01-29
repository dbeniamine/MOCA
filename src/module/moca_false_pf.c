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
#define MOCA_DEBUG

#include <linux/spinlock.h>
#include "moca_false_pf.h"
#include "moca_hashmap.h"
#define MOCA_FALSE_PF_HASH_BITS 14



/*
 * If Moca_use_false_pf == 0, all of these functions directly returns without
 * doing anything. It can be set via the module parameter
 */
int Moca_use_false_pf=1;


typedef struct _Moca_falsePf
{
    void *key; //pte
    int next;
    void *mm;
}*Moca_FalsePf;

hash_map Moca_falsePfMap;
spinlock_t Moca_falsePfLock;

int Moca_FalsePfComparator(hash_entry e1, hash_entry e2)
{
    Moca_FalsePf p1=(Moca_FalsePf)e1,p2=(Moca_FalsePf)e2;
    if(p1->key==p2->key && p1->mm==p2->mm )
        return 0;
    return 1;
}

void Moca_InitFalsePf(void)
{
    if(!Moca_use_false_pf)
        return;
    Moca_falsePfMap=Moca_InitHashMap(MOCA_FALSE_PF_HASH_BITS,
            2*(1<<MOCA_FALSE_PF_HASH_BITS),sizeof(struct _Moca_falsePf),
            Moca_FalsePfComparator);
    spin_lock_init(&Moca_falsePfLock);
}

// Mark pte as not present, and save it as the false page fault
void Moca_AddFalsePf(struct mm_struct *mm, pte_t *pte)
{
    int status;
    struct _Moca_falsePf tmpPf;
    Moca_FalsePf p;

    if(!Moca_use_false_pf || !pte_present(*pte))
        return;

    tmpPf.key=pte;
    tmpPf.mm=mm;
    spin_lock(&Moca_falsePfLock);
    p=(Moca_FalsePf)Moca_AddToMap(Moca_falsePfMap,(hash_entry)&tmpPf,&status);
    spin_unlock(&Moca_falsePfLock);
    switch(status)
    {
        case MOCA_HASHMAP_FULL:
            Moca_Panic("Moca Too many falsepf");
            break;
        case MOCA_HASHMAP_ERROR:
            Moca_Panic("Moca unhandeled hashmap error");
            break;
        case  MOCA_HASHMAP_ALREADY_IN_MAP:
            Moca_Panic("Moca Adding an already exixsting false pf");
            break;
        default:
            //normal add
            pte_clear_flags(*pte,_PAGE_PRESENT);
            MOCA_DEBUG_PRINT("Moca Added false PF %p %p\n", pte, mm);
            break;
    }
}

/*
 * Try to fix false pte fault on pte.
 * Does nothing if pte isn't in the false pte list
 * returns 0 on success
 *         1 if pte is not in the false pf list
 */
int Moca_FixFalsePf(struct mm_struct *mm, pte_t *pte)
{
    struct _Moca_falsePf tmpPf;
    int res=1;
    if(!Moca_use_false_pf)
        return 0;

    tmpPf.key=pte;
    tmpPf.mm=mm;
    spin_lock(&Moca_falsePfLock);
    if(Moca_RemoveFromMap(Moca_falsePfMap,(hash_entry)&tmpPf))
    {
        pte_set_flags(*pte,_PAGE_PRESENT);
        MOCA_DEBUG_PRINT("Moca fixing false pte_fault %p mm %p\n",pte,mm);
        res=0;
    }
    spin_unlock(&Moca_falsePfLock);
    return res;
}


/*
 * Remove all false page faults associated to mm and set the present flags
 * back
 */
void Moca_FixAllFalsePf(struct mm_struct *mm)
{
    int i=0;
    Moca_FalsePf p;
    if(!Moca_use_false_pf)
        return;
    spin_lock(&Moca_falsePfLock);
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
    {
        if(p->mm==mm)
        {
            pte_set_flags(*( pte_t *)(p->key),_PAGE_PRESENT);
            MOCA_DEBUG_PRINT("Moca fixing false pte_fault %p mm %p\n",p->key,mm);
            Moca_RemoveFromMap(Moca_falsePfMap,(hash_entry)p);
        }
    }
    spin_unlock(&Moca_falsePfLock);
}
