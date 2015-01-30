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
#include <asm/atomic.h>
#include "moca_false_pf.h"
#include "moca_hashmap.h"

#define MOCA_FALSE_PF_HASH_BITS 14
#define MOCA_FALSE_PF_VALID 0
#define MOCA_FALSE_PF_BAD 1



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
    int status;
}*Moca_FalsePf;

hash_map Moca_falsePfMap;

//Synchronization stuff
spinlock_t Moca_fpfWLock;
void Moca_FpfPreRead(void);
void Moca_FpfPostRead(void);
void Moca_FpfPreWrite(void);
void Moca_FpfPostWrite(void);

int Moca_FalsePfComparator(hash_entry e1, hash_entry e2)
{
    Moca_FalsePf p1=(Moca_FalsePf)e1,p2=(Moca_FalsePf)e2;
    if(p1->key==p2->key && p1->mm==p2->mm )
        return 0;
    return 1;
}

//Remove all "BAD" falsepf
void Moca_DeleteBadFpf(void)
{
    int i=0;
    Moca_FalsePf p;
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
        if(p->status==MOCA_FALSE_PF_BAD)
            Moca_RemoveFromMap(Moca_falsePfMap,(hash_entry)p);
}

void Moca_InitFalsePf(void)
{
    if(!Moca_use_false_pf)
        return;
    Moca_falsePfMap=Moca_InitHashMap(MOCA_FALSE_PF_HASH_BITS,
            2*(1<<MOCA_FALSE_PF_HASH_BITS),sizeof(struct _Moca_falsePf),
            Moca_FalsePfComparator);
    spin_lock_init(&Moca_fpfWLock);
}

void Moca_DoAddFalsePf(struct mm_struct *mm, pte_t *pte)
{
    int status, try=0;
    struct _Moca_falsePf tmpPf;
    Moca_FalsePf p;
    if(!pte || pte_none(*pte) || !pte_present(*pte))
        return;

    tmpPf.key=pte;
    tmpPf.mm=mm;
    do{
        p=(Moca_FalsePf)Moca_AddToMap(Moca_falsePfMap,(hash_entry)&tmpPf,&status);
        switch(status)
        {
            case MOCA_HASHMAP_FULL:
                //TODO: clean BAD
                Moca_DeleteBadFpf();
                ++try;
                break;
            case MOCA_HASHMAP_ERROR:
                Moca_Panic("Moca unhandeled hashmap error");
                return;
            case  MOCA_HASHMAP_ALREADY_IN_MAP:
                //TODO update status
                if(p->status!=MOCA_FALSE_PF_BAD)
                {
                    Moca_Panic("Moca adding false pf already in map");
                    return;
                }
                MOCA_DEBUG_PRINT("Moca Reusing bad false PF %p %p\n", pte, mm);
            default:
                //normal add
                p->status=MOCA_FALSE_PF_VALID;
                pte_clear_flags(*pte,_PAGE_PRESENT);
                MOCA_DEBUG_PRINT("Moca Added false PF %p %p\n", pte, mm);
                return;
        }
    }while(try<2);
    MOCA_DEBUG_PRINT("Moca more than two try to add false pf pte %p, mm %p\n",
            pte, mm);
}

// Mark pte as not present, and save it as the false page fault
void Moca_AddFalsePf(struct mm_struct *mm, pte_t **buf, int nb)
{
    int i;
    if(!Moca_use_false_pf)
        return;
    Moca_FpfPreWrite();
    for(i=0;i<nb;++i)
        Moca_DoAddFalsePf(mm,buf[i]);
    Moca_FpfPostWrite();
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
    Moca_FalsePf p;
    int res=1;
    if(!Moca_use_false_pf)
        return 0;

    Moca_FpfPreRead();
    MOCA_DEBUG_PRINT("Moca testing pte fault %p mm %p\n",pte,mm);
    tmpPf.key=pte;
    tmpPf.mm=mm;
    if((p=(Moca_FalsePf)Moca_EntryFromKey(Moca_falsePfMap,(hash_entry)&tmpPf)))
    {
        p->status=MOCA_FALSE_PF_BAD;
        pte_set_flags(*pte,_PAGE_PRESENT);
        MOCA_DEBUG_PRINT("Moca fixing false pte_fault %p mm %p\n",pte,mm);
        res=0;
    }
    Moca_FpfPostRead();
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
    Moca_FpfPreWrite();
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
    {
        if(p->mm==mm)
        {
            pte_set_flags(*( pte_t *)(p->key),_PAGE_PRESENT);
            MOCA_DEBUG_PRINT("Moca fixing false pte_fault %p mm %p\n",p->key,mm);
            Moca_RemoveFromMap(Moca_falsePfMap,(hash_entry)p);
        }
    }
    Moca_FpfPostWrite();
}

atomic_t Moca_fpfNbR=ATOMIC_INIT(0);
atomic_t Moca_fpfNbW=ATOMIC_INIT(0);

//Ugly but here we really can't yield the proc
void Moca_ActiveWait(void)
{
    int i=0;
    while(i<1000000000){++i;}
}

void Moca_FpfPreRead(void)
{
    while(atomic_read(&Moca_fpfNbW))
        Moca_ActiveWait();
    atomic_inc(&Moca_fpfNbR);
}
void Moca_FpfPostRead(void)
{
    atomic_dec(&Moca_fpfNbR);
}
void Moca_FpfPreWrite(void)
{
    atomic_inc(&Moca_fpfNbW);
    while(atomic_read(&Moca_fpfNbR) )
        Moca_ActiveWait();
    spin_lock(&Moca_fpfWLock);
}
void Moca_FpfPostWrite(void)
{
    spin_unlock(&Moca_fpfWLock);
    atomic_dec(&Moca_fpfNbW);
}

