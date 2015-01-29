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

int main()
{
    //Init
    int i, nbAdd, *ptr,st, j, k;
    foo e, keys;
    hash_map map=Moca_InitHashMap(14,2*(1UL<<14), sizeof(struct _foo),multicomp);
    srand(time(NULL));

    nbAdd=1000;
    keys=calloc(nbAdd, sizeof(struct _foo));
    for(k=0;k<10;++k)
    {
        printf("Inserting %d elements\n", nbAdd);
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
            printf("inserting e %p, k %p, n: %d, v %d, ov %d\n", keys+j, keys[j].key,
                    keys[j].next,keys[j].val, keys[j].other_val);
            e= (foo)Moca_AddToMap(map,(hash_entry)(keys+j), &st);
            /* printf("inserting %p, %d/%d res: %p / %d\n", ptr, i, nbAdd, e, st); */
            printf("inserted e %p, k %p, n: %d, v %d, ov %d\n", e, e->key,
                    e->next,e->val, e->other_val);
            printf("%d elements in map\n", Moca_NbElementInMap(map));
        }
        printf("All insert done, table size :%d\n", Moca_NbElementInMap(map));
        for(i=0;i<nbAdd;i++)
        {
            j=Moca_PosInMap(map, (hash_entry)(keys+i));
            printf("key %p at pos %d\n",keys[i].key,j);
            e=(foo)Moca_EntryAtPos(map,(unsigned)j);
            printf("Entry at pos %d, %p %p %d %d %d\n", j,e, e->key, e->next,
                    e->val, e->other_val);
        }
        printf("All position checked, table size :%d\n", Moca_NbElementInMap(map));
        for(i=0;i<nbAdd/2;i++)
        {
            e=(foo)Moca_RemoveFromMap(map, (hash_entry)(keys+i));
            printf("Removing key %p %p %p %d %d %d\n", keys+i, e,
                    e->key, e->next, e->val,e->other_val);
            printf("Still %d elements \n", Moca_NbElementInMap(map));
            free(e->key);
            keys[i].key=NULL;
        }
        printf("half remove done, table size :%d\n", Moca_NbElementInMap(map));
        i=0;
        while((e=(foo)Moca_NextEntryPos(map, (unsigned int *)&i)))
        {
            printf("Entry at pos %d, %p %p %d %d %d\n", i-1,e, e->key, e->next,
                    e->val, e->other_val);
            Moca_RemoveFromMap(map,(hash_entry)(e));
            free(e->key);
            j=0;
            while(multicomp((hash_entry)e, (hash_entry)(keys+j))!=0)
                ++j;
            keys[j].key=NULL;
        }
        printf("All remove done, table size :%d\n", Moca_NbElementInMap(map));
    }
    free(keys);
    Moca_FreeMap(map);
    return 0;
}
