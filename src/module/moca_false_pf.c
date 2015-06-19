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
/* #define MOCA_DEBUG */

#include "moca_false_pf.h"
#include "moca_hashmap.h"
#include <linux/rwlock_types.h>
#include <linux/rwlock.h>

#define MOCA_FALSE_PF_HASH_BITS 14
#define MOCA_FALSE_PF_VALID 0
#define MOCA_FALSE_PF_BAD 1



/*
 * If Moca_use_false_pf == 0, all of these functions directly returns without
 * doing anything. It can be set via the module parameter
 */
int Moca_use_false_pf=1;
int Moca_false_pf_ugly=0;


typedef struct _Moca_falsePf
{
    void *key; //pte
    int next;
    void *mm;
    int status;
}*Moca_FalsePf;

hash_map Moca_falsePfMap;
DEFINE_RWLOCK(Moca_fpfRWLock);

//Synchronization stuff

int Moca_FixPte(pte_t *pte, struct mm_struct *mm)
{
    int res=1;
    if(!pte || pte_none(*pte))
        return res;
    *pte=pte_set_flags(*pte, _PAGE_PRESENT);
    MOCA_DEBUG_PRINT("Moca fixing false pte_fault %p mm %p\n",pte,mm);
    res=0;
    return res;
}

void Moca_FixAllPte(struct mm_struct *mm)
{
    pgd_t *pgd=mm->pgd;
    pmd_t *pmd;
    pud_t *pud;
    pte_t *pte;
    int i,j,k,l;
    MOCA_DEBUG_PRINT("MemMap fixing all pte from mm %p\n", mm);
    for (i=0;i<PTRS_PER_PGD;++i)
    {
        if(!pgd_none(pgd[i]) && pgd_present(pgd[i]))
        {
            pud=pud_offset(pgd+i,0);
            for (j=0;j<PTRS_PER_PUD;++j)
            {
                pmd=pmd_offset(pud +i, 0);
                for (k=0;k<PTRS_PER_PMD;++k)
                {
                    if(!pmd_none(pmd[k]) && pmd_present(pmd[k]))
                    {
                        pte=(pte_t *)(pmd+k);
                        for(l=0;l<PTRS_PER_PTE;++l)
                            if(!pte_none(pte[l]) && !pte_present(pte[l]))
                                Moca_FixPte(pte+l, mm);
                    }
                }
            }
        }
    }
}

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
    if(!Moca_use_false_pf)
        return;
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
    {
        if(p->status==MOCA_FALSE_PF_BAD)
        {
            MOCA_DEBUG_PRINT("Moca removed bad pf %p %p\n", p->key, p->mm);
            Moca_RemoveFromMap(Moca_falsePfMap,(hash_entry)p);
        }
    }
}

void Moca_InitFalsePf(void)
{
    if(!Moca_use_false_pf  || Moca_false_pf_ugly)
        return;
    Moca_falsePfMap=Moca_InitHashMap(MOCA_FALSE_PF_HASH_BITS,
            2*(1<<MOCA_FALSE_PF_HASH_BITS),sizeof(struct _Moca_falsePf),
            &Moca_FalsePfComparator);
    rwlock_init(&Moca_fpfRWLock);
}

void Moca_ClearFalsePfData(void)
{
    int i;
    Moca_FalsePf p;
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    MOCA_DEBUG_PRINT("Moca removing all false pf\n");
    write_lock(&Moca_fpfRWLock);
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
        Moca_FixPte((pte_t *)p->key, NULL);
    MOCA_DEBUG_PRINT("Moca removing all false map%p\n",Moca_falsePfMap);
    Moca_FreeMap(Moca_falsePfMap);
    write_unlock(&Moca_fpfRWLock);
}

void Moca_AddFalsePf(struct mm_struct *mm, pte_t *pte)
{
    int status, try=0;
    struct _Moca_falsePf tmpPf;
    Moca_FalsePf p;
    if(!Moca_use_false_pf || pte_none(*pte))
        return;
    if(Moca_false_pf_ugly)
    {
        *pte=pte_clear_flags(*pte,_PAGE_PRESENT);
        return;
    }

    tmpPf.key=pte;
    tmpPf.mm=mm;
    write_lock(&Moca_fpfRWLock);
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
                write_unlock(&Moca_fpfRWLock);
                Moca_Panic("Moca unhandled hashmap error");
                return;
            case  MOCA_HASHMAP_ALREADY_IN_MAP:
                MOCA_DEBUG_PRINT("Moca Reusing bad false PF %p %p\n", pte, mm);
            default:
                //normal add
                MOCA_DEBUG_PRINT("Moca Added false PF %p %p at %d/%d \n", pte, 
                        mm,(unsigned)status, Moca_NbElementInMap(Moca_falsePfMap));
                p->status=MOCA_FALSE_PF_VALID;
                *pte=pte_clear_flags(*pte,_PAGE_PRESENT);
                write_unlock(&Moca_fpfRWLock);
                return;
        }
    }while(try<2);
    MOCA_DEBUG_PRINT("Moca more than two try to add false pf pte %p, mm %p\n",
            pte, mm);
    write_unlock(&Moca_fpfRWLock);
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
    if(!Moca_use_false_pf || !pte || pte_none(*pte))
        return res;
    if(Moca_false_pf_ugly)
        return Moca_FixPte(pte,mm);

    read_lock(&Moca_fpfRWLock);
    MOCA_DEBUG_PRINT("Moca testing pte fault %p mm %p\n",pte,mm);
    tmpPf.key=pte;
    tmpPf.mm=mm;
    if((p=(Moca_FalsePf)Moca_EntryFromKey(Moca_falsePfMap,(hash_entry)&tmpPf)))
    {
        MOCA_DEBUG_PRINT("Moca found false pte %p mm %p\n",p->key,p->mm);
        res=Moca_FixPte(pte, mm);
        p->status=MOCA_FALSE_PF_BAD;
    }
    read_unlock(&Moca_fpfRWLock);
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
    if(!Moca_use_false_pf || !mm)
        return;
    if(Moca_false_pf_ugly)
    {
        Moca_FixAllPte(mm);
        return;
    }
    write_lock(&Moca_fpfRWLock);
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
    {
        if(p->mm==mm)
        {
            Moca_FixPte((pte_t *)p->key, mm);
            Moca_RemoveFromMap(Moca_falsePfMap,(hash_entry)p);
        }
    }
    write_unlock(&Moca_fpfRWLock);
}
