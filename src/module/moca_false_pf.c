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
#include <linux/spinlock.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>

#define MOCA_FALSE_PF_HASH_BITS 15
#define MOCA_FALSE_PF_VALID 0
#define MOCA_FALSE_PF_BAD 1



/*
 * If Moca_use_false_pf == 0, all of these functions directly returns without
 * doing anything. It can be set via the module parameter
 */
int Moca_use_false_pf=1;
int Moca_false_pf_ugly=0;
unsigned long *Moca_recentlyFixed;


typedef struct _Moca_falsePf
{
    void *key; //addr
    int next;
    void *mm;
    int status;
}*Moca_FalsePf;

hash_map Moca_falsePfMap;
DEFINE_RWLOCK(Moca_fpfRWLock);


int Moca_RecentlyFixed(unsigned long addr)
{
    int i=0;
    while(i<NR_CPUS)
    {
        if(Moca_recentlyFixed[i]==(addr&PAGE_MASK))
            return 0;
        ++i;
    }
    return 1;
}

void *Moca_PhyFromVirt(void *addr, struct mm_struct *mm)
{
    return addr;
    /* pte_t *pte=Moca_PteFromAdress((unsigned long)addr,mm); */
    /* if(!pte || pte_none(*pte)) */
    /*     return addr; //Kernel address no translation needed */
    /* return (void *)__pa(pte_page(*pte)); */
}

static void Moca_UnmapPte(pte_t *pte,spinlock_t *ptl)
{
    if(pte)
        pte_unmap(pte);
}


static pte_t *Moca_PteFromAdress(unsigned long address, struct mm_struct *mm,
        spinlock_t **ptl)
{
    pgd_t *pgd;
    pmd_t *pmd;
    pud_t *pud;
    pte_t *pte;
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
    pte=pte_offset_map(pmd, address);
    if(pte && pte_none(*pte))
    {
        Moca_UnmapPte(pte,*ptl);
        return NULL;
    }
    return pte;

}

static int Moca_FixPte(pte_t *pte, struct mm_struct *mm)
{
    int res=1;
    if(!pte || pte_none(*pte))
        return res;
    *pte=pte_set_flags(*pte, _PAGE_PRESENT);
    MOCA_DEBUG_PRINT("Moca fixing false pte_fault %p mm %p\n",pte,mm);
    res=0;
    return res;
}

static int Moca_ClearAddr(unsigned long addr,struct mm_struct *mm)
{
    spinlock_t *ptl;
    pte_t *pte;
    if(Moca_RecentlyFixed(addr)==0)
        return 1;
    pte=Moca_PteFromAdress(addr,mm,&ptl);
    if(!pte)
        return 1;
    MOCA_DEBUG_PRINT("Moca clearing pte %p addr %lx mm %p\n",pte,addr,mm);
    *pte=pte_clear_flags(*pte,_PAGE_PRESENT);
    Moca_UnmapPte(pte,ptl);
    return 0;
}

static int Moca_FixAddr(unsigned long addr,struct mm_struct *mm, int cpu)
{
    spinlock_t *ptl;
    int ret=1;
    pte_t *pte=Moca_PteFromAdress(addr,mm,&ptl);
    if(!pte)
        return ret;
    MOCA_DEBUG_PRINT("Moca fixing pte %p addr %lx mm %p\n",pte,addr,mm);
    if((ret=Moca_FixPte(pte,mm))==0)
        Moca_recentlyFixed[cpu]=addr&PAGE_MASK;
    Moca_UnmapPte(pte,ptl);
    return ret;
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

int Moca_ShouldRemoveEntry(void *e)
{
    Moca_FalsePf p=(Moca_FalsePf)e;
    if (p->status==MOCA_FALSE_PF_BAD)
    {
        MOCA_DEBUG_PRINT("Moca removed bad pf %p %p\n", p->key, p->mm);
        return 0;
    }
    return 1;
}

//Remove all "BAD" falsepf
void Moca_DeleteBadFpf(void)
{
    if(!Moca_use_false_pf)
        return;
    Moca_ConditionalRemove(Moca_falsePfMap,Moca_ShouldRemoveEntry);
}

void Moca_FalsePfInitializer(void *e)
{
    Moca_FalsePf f=(Moca_FalsePf)e;
    f->status=0;
}

void Moca_InitFalsePf(void)
{
    if(!Moca_use_false_pf  || Moca_false_pf_ugly)
        return;
    Moca_falsePfMap=Moca_InitHashMap(MOCA_FALSE_PF_HASH_BITS,
            2*(1<<MOCA_FALSE_PF_HASH_BITS),sizeof(struct _Moca_falsePf),
            &Moca_FalsePfComparator, &Moca_FalsePfInitializer);
    rwlock_init(&Moca_fpfRWLock);
    Moca_recentlyFixed=kcalloc(NR_CPUS,sizeof(unsigned long),GFP_KERNEL);
}

void Moca_ClearFalsePfData(void)
{
    int i;
    Moca_FalsePf p;
    if(!Moca_use_false_pf || Moca_false_pf_ugly)
        return;
    MOCA_DEBUG_PRINT("Moca removing all false pf\n");
    while((p=(Moca_FalsePf)Moca_NextEntryPos(Moca_falsePfMap, &i))!=NULL)
        if(p->status==MOCA_FALSE_PF_VALID)
        {
            if(Moca_FixAddr((unsigned long)p->key, NULL,current->on_cpu)==0)
                p->status=MOCA_FALSE_PF_BAD;
        }
    MOCA_DEBUG_PRINT("Moca removing all false map%p\n",Moca_falsePfMap);
    Moca_FreeMap(Moca_falsePfMap);
}

void Moca_AddFalsePf(struct mm_struct *mm, unsigned long address)
{
    int status, try=0;
    struct _Moca_falsePf tmpPf;
    Moca_FalsePf p;
    if(!Moca_use_false_pf || !mm)
        return;
    if(Moca_false_pf_ugly)
    {
        Moca_ClearAddr(address,mm);
        return;
    }

    tmpPf.key=(void*)(address&PAGE_MASK);
    tmpPf.mm=mm;
    do{
        p=(Moca_FalsePf)Moca_AddToMap(Moca_falsePfMap,(hash_entry)&tmpPf,&status);
        switch(status)
        {
            case MOCA_HASHMAP_FULL:
                Moca_DeleteBadFpf();
                ++try;
                break;
            case MOCA_HASHMAP_ERROR:
                Moca_Panic("Moca unhandled hashmap error");
                return;
            case  MOCA_HASHMAP_ALREADY_IN_MAP:
                MOCA_DEBUG_PRINT("Moca Reusing bad false PF %lx %p\n", address, mm);
            default:
                //normal add
                MOCA_DEBUG_PRINT("Moca Added false PF %lx %p at %d/%d \n", address,
                        mm,(unsigned)status, Moca_NbElementInMap(Moca_falsePfMap));
                p->status=MOCA_FALSE_PF_VALID;
                if(Moca_ClearAddr((unsigned long)p->key,mm)!=0)
                    p->status=MOCA_FALSE_PF_BAD;
                return;
        }
    }while(try<2);
    MOCA_DEBUG_PRINT("Moca more than two try to add false pf addr %lx, mm %p\n",
            address, mm);
}

/*
 * Try to fix false fault on addr.
 * Does nothing if addr isn't in the false list
 * returns 0 on success
 *         1 if addr is not in the false pf list
 */
int Moca_FixFalsePf(struct mm_struct *mm, unsigned long address, int cpu)
{
    struct _Moca_falsePf tmpPf;
    Moca_FalsePf p;
    int res=1;
    if(!Moca_use_false_pf)
        return res;
    if(Moca_false_pf_ugly)
        return Moca_FixAddr(address,mm,cpu);

    MOCA_DEBUG_PRINT("Moca testing addr fault %lx mm %p\n",address,mm);
    tmpPf.key=(void*)(address&PAGE_MASK);
    tmpPf.mm=mm;
    if((p=(Moca_FalsePf)Moca_EntryFromKey(Moca_falsePfMap,(hash_entry)&tmpPf)))
    {
        MOCA_DEBUG_PRINT("Moca found false pf %p mm %p\n",p->key,p->mm);
        if(p->status == MOCA_FALSE_PF_VALID)
        {
            if((res=Moca_FixAddr((unsigned long)p->key, mm,cpu))==0)
                p->status=MOCA_FALSE_PF_BAD;
        }
    }
    return res;
}


/*
 * Remove all false page faults associated to mm and set the present flags
 * back
 */
void Moca_FixAllFalsePf(struct mm_struct *mm, int cpu)
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
        if(p->mm==mm)
        {
            if(Moca_FixAddr((unsigned long)p->key, mm,cpu)==0)
                p->status=MOCA_FALSE_PF_BAD;
        }
    }
}

void Moca_WLockPf(void)
{
    if(!Moca_use_false_pf)
        return;
    write_lock(&Moca_fpfRWLock);
}

void Moca_WUnlockPf(void)
{
    if(!Moca_use_false_pf)
        return;
    write_unlock(&Moca_fpfRWLock);

}

void Moca_RLockPf(void)
{
    if(!Moca_use_false_pf)
        return;
    read_lock(&Moca_fpfRWLock);
}

void Moca_RUnlockPf(void)
{
    if(!Moca_use_false_pf)
        return;
    read_unlock(&Moca_fpfRWLock);
}
