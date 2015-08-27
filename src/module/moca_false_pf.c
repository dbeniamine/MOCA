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
#include <linux/delay.h> //msleep

#define MOCA_FALSE_PF_HASH_BITS 16
#define MOCA_FALSE_PF_VALID 0
#define MOCA_FALSE_PF_BAD 1

/*
 * If Moca_use_false_pf == 0, all of these functions directly returns without
 * doing anything. It can be set via the module parameter
 */
int Moca_use_false_pf=1;
int Moca_false_pf_ugly=0;
atomic_t Moca_fpf_writers=ATOMIC_INIT(0);


typedef struct _Moca_falsePf
{
    void *key; //addr
    int next;
    int status;
}*Moca_FalsePf;

hash_map Moca_falsePfMap;
DEFINE_RWLOCK(Moca_fpfRWLock);

static inline int Moca_BadPte(pte_t *pte)
{
    return (!pte || pte_none(*pte));
}




void *Moca_PhyFromVirt(void *addr, struct mm_struct *mm)
{
    return addr;
    /* pte_t *pte=Moca_PteFromAddress((unsigned long)addr,mm); */
    /* if(!pte || pte_none(*pte)) */
    /*     return addr; //Kernel address no translation needed */
    /* return (void *)__pa(pte_page(*pte)); */
}

pte_t *Moca_PteFromAddress(unsigned long address, struct mm_struct *mm)
{
    pgd_t *pgd;
    pmd_t *pmd;
    pud_t *pud;
    if(!mm)
    {
        MOCA_DEBUG_PRINT("Moca mm null !\n");
        return NULL;
    }
    pgd = pgd_offset(mm, address);
    if (!pgd || pgd_none(*pgd) || pgd_bad(*pgd) )
        return NULL;
    pud = pud_offset(pgd, address);
    if(!pud || pud_none(*pud) || pud_bad(*pud))
        return NULL;
    pmd = pmd_offset(pud, address);
    if (!pmd || pmd_none(*pmd) || pmd_bad(*pmd))
        return NULL;
    return pte_offset_map(pmd, address);
}

static inline void Moca_GetCountersFromPte(pte_t *pte,int *young, int* dirty)
{
    *young=pte_young(*pte);
    *dirty=pte_dirty(*pte);
}

void Moca_GetCountersFromAddr(unsigned long addr, struct mm_struct *mm,
        int * young, int *dirty)
{
    pte_t *pte=Moca_PteFromAddress(addr,mm);
    if(Moca_BadPte(pte))
    {
        *young=0;
        *dirty=0;
        return;
    }
    Moca_GetCountersFromPte(pte,young,dirty);
    pte_unmap(pte);
}

int Moca_clearPte(unsigned long addr, struct mm_struct *mm,
        int *young,int *dirty)
{
    pte_t *pte=Moca_PteFromAddress(addr,mm);
    if(!Moca_BadPte(pte))
    {
        Moca_GetCountersFromPte(pte,young,dirty);
        *pte=pte_clear_flags(*pte, _PAGE_PRESENT);
        pte_unmap(pte);
        MOCA_DEBUG_PRINT("Moca cleared pte_fault %p mm %p\n",pte,mm);
        return 0;
    }
    return 1;
}


int Moca_FixPte(unsigned long addr, struct mm_struct *mm)
{
    pte_t *pte=Moca_PteFromAddress(addr,mm);
    if(!Moca_BadPte(pte))
    {
        MOCA_DEBUG_PRINT("Moca fixing false pte_fault addr %p, pte %p mm %p\n",
                (void *)addr,pte,mm);
        *pte=pte_set_flags(*pte, _PAGE_PRESENT);
        pte_unmap(pte);
        MOCA_DEBUG_PRINT("Moca fixed false pte_fault addr %p, pte %p mm %p\n",
                (void *)addr,pte,mm);
        return 0;
    }
    MOCA_DEBUG_PRINT("Moca not fixing false pte_fault addr %p pte %p mm %p\n",
            (void *)addr,pte,mm);
    return 1;
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
                                *pte=pte_set_flags(*pte, _PAGE_PRESENT);
                    }
                }
            }
        }
    }
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
            MOCA_DEBUG_PRINT("Moca removed bad pf %p %p\n", (void *)p->key, p->mm);
            Moca_RemoveFromMap(Moca_falsePfMap,p->key);
        }
    }
}

void Moca_InitFalsePf(void)
{
    if(!Moca_use_false_pf  || Moca_false_pf_ugly)
        return;
    Moca_falsePfMap=Moca_InitHashMap(MOCA_FALSE_PF_HASH_BITS,
            2*(1<<MOCA_FALSE_PF_HASH_BITS),sizeof(struct _Moca_falsePf));
    rwlock_init(&Moca_fpfRWLock);
}

void Moca_ClearFalsePf(void)
{
    int i;
    Moca_FalsePf p;
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    MOCA_DEBUG_PRINT("Moca removing all false pf\n");
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
    {
        Moca_FixPte((unsigned long)p->key, NULL);
        Moca_RemoveFromMap(Moca_falsePfMap,p->key);
    }
}

void Moca_ClearFalsePfData(void)
{
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    MOCA_DEBUG_PRINT("Moca removing all false map%p\n",Moca_falsePfMap);
    Moca_FreeMap(Moca_falsePfMap);
}

void Moca_AddFalsePf(struct mm_struct *mm, unsigned long address, int *young,
        int * dirty)
{
    int status, try=0;
    unsigned long addr=address&PAGE_MASK;
    Moca_FalsePf p;
    *young=0;
    *dirty=0;
    if(!Moca_use_false_pf)
        return;
    if(Moca_false_pf_ugly)
    {
        Moca_clearPte(addr,mm,young,dirty);
        return;
    }

    do{
        p=(Moca_FalsePf)Moca_AddToMap(Moca_falsePfMap,(void *)addr,&status);
        switch(status)
        {
            case MOCA_HASHMAP_FULL:
                //TODO: clean BAD
                Moca_DeleteBadFpf();
                ++try;
                break;
            case MOCA_HASHMAP_ERROR:
                Moca_Panic("Moca unhandled hashmap error");
                return;
            case  MOCA_HASHMAP_ALREADY_IN_MAP:
                MOCA_DEBUG_PRINT("Moca Reusing bad false PF %p %p\n", (void *)addr, mm);
                break;
            default:
                //normal add
                MOCA_DEBUG_PRINT("Moca Added false PF %p %p at %d/%d \n", (void *)addr, 
                        mm,(unsigned)status, Moca_NbElementInMap(Moca_falsePfMap));
                break;
        }
    }while(status==MOCA_HASHMAP_FULL && try<2);
    if(status==MOCA_HASHMAP_FULL)
    {
    MOCA_DEBUG_PRINT("Moca more than two try to add false pf addr %p, mm %p\n",
            (void *)addr, mm);
    }
    else
    {
       p->status=MOCA_FALSE_PF_VALID;
       Moca_clearPte(addr,mm,young,dirty);
    }
}

/*
 * Try to fix false pte fault on pte.
 * Does nothing if pte isn't in the false pte list
 * returns 0 on success
 *         1 if pte is not in the false pf list
 */
int Moca_FixFalsePf(struct mm_struct *mm, unsigned long address)
{
    unsigned long addr=address&PAGE_MASK;
    Moca_FalsePf p;
    int res=1;
    if(!Moca_use_false_pf)
        return res;
    if(Moca_false_pf_ugly)
        return Moca_FixPte(addr,mm);

    MOCA_DEBUG_PRINT("Moca testing addr fault %p mm %p\n",(void *)addr,mm);
    if((p=(Moca_FalsePf)Moca_EntryFromKey(Moca_falsePfMap,(void *)addr)))
    {
        MOCA_DEBUG_PRINT("Moca found false addr %p mm %p status %d \n",
                (void *)p->key,p->mm,p->status);
        if(p->status == MOCA_FALSE_PF_VALID)
        {
            if((res=Moca_FixPte(addr, mm))==0)
                p->status=MOCA_FALSE_PF_BAD;
        }
    }
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
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
    {
        if(Moca_FixPte((unsigned long)p->key, mm)==0)
            p->status=MOCA_FALSE_PF_BAD;
    }
}


// Rwlock with priority to writes
void Moca_WLockPf(void)
{
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    atomic_inc(&Moca_fpf_writers);
    write_lock(&Moca_fpfRWLock);
}

void Moca_WUnlockPf(void)
{
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    atomic_dec(&Moca_fpf_writers);
    write_unlock(&Moca_fpfRWLock);
}

void Moca_RLockPf(void)
{
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    while(atomic_read(&Moca_fpf_writers)>0){msleep(5);}
    read_lock(&Moca_fpfRWLock);
}

void Moca_RUnlockPf(void)
{
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    read_unlock(&Moca_fpfRWLock);
}
