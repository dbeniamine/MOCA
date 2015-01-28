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
#include "moca_false_pf.h"
#include "moca_hashmap.h"
#include <linux/spinlock.h>
#define MOCA_FALSE_PF_HASH_BITS 14
//#define MOCA_DEBUG_PRINT


/*
 * If Moca_use_false_pf == 0, all of these functions directly returns without
 * doing anything. It can be set via the module parameter
 */
int Moca_use_false_pf=0;

typedef struct _Moca_falsePf
{
    void *key; //pte
    int next;
    void *mm;
}*Moca_FalsePf;

hash_map Moca_FalsePfMap;
spinlock_t Mocal_FalsePfLock;

int Moca_FalsePfComparator(hash_entry e1, hash_entry e2)
{
    Moca_FalsePf p1=(Moca_FalsePf)e1,p2=(Moca_FalsePf)p2;
    if(p1->key==p2->key && p1->mm==p2->mm )
        return 0;
    return 1;
}

void Moca_InitFalsePf(void)
{
    Moca_FalsePfMap=Moca_InitHashMap(MOCA_FALSE_PF_HASH_BITS,
            2*(1<<MOCA_FALSE_PF_HASH_BITS),sizeof(struct _Moca_falsePf),
            Moca_FalsePfComparator);
    spin_lock_init(&Moca_FalsePfLock);
}
// Clear present flag for pte and not that pte is actually present
int Moca_AddFalsePf(struct mm_struct *mm, pte_t pte)
{
    int status;
    Moca_FalsePf p;
    spin_lock(&Moca_FalsePfLock);
    p=Moca_(FalsePf)Moca_AddToMap(Moca_FalsePf,pte,&status);
    spin_unlock(&Moca_FalsePfLock);
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
            p->mm=mm;
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
int Moca_DelFalsePf(struct mm_struct *mm, pte_t pte)
{
    //TODO
    spin_lock(&Moca_FalsePfLock);
    spin_unlock(&Moca_FalsePfLock);
    return 0;
}


//TODO clear all false pf from mm
void Mocal_DelAllPf(struct mm_struct *mm)
{
    spin_lock(&Moca_FalsePfLock);
    spin_unlock(&Moca_FalsePfLock);
}
