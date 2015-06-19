#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "moca.h"
#include "moca_hashmap.h"

typedef struct _foo
{
    void *key;
    int next;
    int val;
    int other_val;
}*foo;


int multicomp(hash_entry e1, hash_entry e2)
{
    foo f1=(foo)e1, f2=(foo)e2;
    if(f1->key==f2->key && f1->val==f2->val && f1->other_val ==f2->other_val)
        return 0;
    return 1;
    //    return (unsigned long)f1->key-(unsigned long)f2->key;
}

#define MAX_LOOP 10
int main()
{
    //Init
    int i, nbAdd, *ptr,st, j, k;
    foo e, keys;
    struct _foo tmp;
    hash_map map=Moca_InitHashMap(14,2*(1UL<<14), sizeof(struct _foo),&multicomp);
    srand(time(NULL));

    nbAdd=(1UL<<14);
    keys=calloc(nbAdd, sizeof(struct _foo));
    for(k=0;k<MAX_LOOP;++k)
    {
        PRINT("Inserting %d elements\n", nbAdd);
        for(i=0;i<nbAdd;i++)
        {
            ptr=malloc(sizeof(int));
            j=rand()%nbAdd;
            while(keys[j].key!=NULL)
            {
                j=(j+1)%nbAdd;
            }
            keys[j].key=ptr;
            keys[j].val=rand()%10;
            keys[j].other_val=rand()%10;
            tmp.key=ptr;
            tmp.val=keys[j].val;
            tmp.other_val=keys[j].other_val;
            PRINT("inserting e %p, k %p, n: %d, v %d, ov %d\n", keys+j, keys[j].key,
                    keys[j].next,keys[j].val, keys[j].other_val);
            e= (foo)Moca_AddToMap(map,(hash_entry)(&tmp), &st);
            if(e){
                /* PRINT("inserting %p, %d/%d res: %p / %d\n", ptr, i, nbAdd, e, st); */
                PRINT("inserted e %p, k %p, n: %d, v %d, ov %d\n", e, e->key,
                        e->next,e->val, e->other_val);
                PRINT("%d elements in map\n", Moca_NbElementInMap(map));
            }
            else{
                free(ptr);
                keys[j].key=NULL;
            }
        }
        PRINT("All insert done, table size :%d\n", Moca_NbElementInMap(map));
        for(i=0;i<nbAdd;i++)
        {
            if(keys[i].key){
                j=Moca_PosInMap(map, (hash_entry)(keys+i));
                PRINT("key %p at pos %d\n",keys[i].key,j);
                e=(foo)Moca_EntryAtPos(map,(unsigned)j);
                PRINT("Entry at pos %d, %p %p %d %d %d\n", j,e, e->key, e->next,
                        e->val, e->other_val);
            }
        }
        PRINT("All position checked, table size :%d\n", Moca_NbElementInMap(map));
        int nbRm=nbAdd/(rand()%10);
        for(i=0;i<nbRm;i++)
        {
            if(keys[i].key){
                e=(foo)Moca_RemoveFromMap(map, (hash_entry)(keys+i));
                PRINT("Removing key %p %p %p %d %d %d\n", keys+i, e,
                        e->key, e->next, e->val,e->other_val);
                PRINT("Still %d elements \n", Moca_NbElementInMap(map));
                keys[i].key=NULL;
            }
        }
        PRINT("half remove done, table size :%d\n", Moca_NbElementInMap(map));
        if(k==MAX_LOOP-1){
            i=0;
            while((e=(foo)Moca_NextEntryPos(map, (unsigned int *)&i)))
            {
                PRINT("Entry at pos %d, %p %p %d %d %d\n", i-1,e, e->key, e->next,
                        e->val, e->other_val);
                Moca_RemoveFromMap(map,(hash_entry)(e));
                free(e->key);
                j=0;
                while(multicomp((hash_entry)e, (hash_entry)(keys+j))!=0)
                    ++j;
                keys[j].key=NULL;
            }
            PRINT("All remove done, table size :%d\n", Moca_NbElementInMap(map));
        }
    }
    free(keys);
    Moca_FreeMap(map);
    return 0;
}
