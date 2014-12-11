#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "memmap.h"
#include "memmap_hashmap.h"

typedef struct _foo
{
    void *key;
    int next;
    int val;
    int other_val;
}*foo;

int main()
{
    //Init
    int i, nbAdd, *ptr,st, **keys,j, k;
    foo e;
    hash_map map=MemMap_InitHashMap(14UL,2, sizeof(struct _foo));
    srand(time(NULL));

    nbAdd=1000;
    keys=calloc(nbAdd, sizeof(int *));
    for(k=0;k<10;++k)
    {
        printf("Inserting %d elements\n", nbAdd);
        for(i=0;i<nbAdd;i++)
        {
            ptr=malloc(sizeof(int));
            j=rand()%nbAdd;
            while(keys[j]!=NULL)
            {
                j=(j+1)%nbAdd;
            }
            keys[j]=ptr;
            /* printf("inserting %p, %d/%d\n", ptr, i, nbAdd); */
            e= (foo)MemMap_AddToMap(map,ptr, &st);
            /* printf("inserting %p, %d/%d res: %p / %d\n", ptr, i, nbAdd, e, st); */
            if(e)
            {
                e->val=rand()%10;
                e->other_val=rand()%10;
            }
            printf("inserted e %p, k %p, n: %d, v %d, ov %d\n", e, e->key,
                    e->next,e->val, e->other_val);
            printf("%d elements in map\n", MemMap_NbElementInMap(map));
        }
        for(i=0;i<nbAdd;i++)
        {
            j=MemMap_PosInMap(map, keys[i]);
            printf("key %p at pos %d\n",keys[i],j);
            e=(foo)MemMap_EntryAtPos(map,(unsigned)j);
            printf("Entry at pos %d, %p %p %d %d %d\n", j,e, e->key, e->next,
                    e->val, e->other_val);
        }
        printf("All insert done, table size :%d\n", MemMap_NbElementInMap(map));
        for(i=0;i<nbAdd;i++)
        {
            e=(foo)MemMap_RemoveFromMap(map, keys[i]);
            printf("Removing key %p %p %p %d %d %d\n", keys[i], e,
                    e->key, e->next, e->val,e->other_val);
            printf("Still %d elements \n", MemMap_NbElementInMap(map));
            free(e->key);
            keys[i]=NULL;
        }
        printf("All remove done, table size :%d\n", MemMap_NbElementInMap(map));
    }
    MemMap_FreeMap(map);
    return 0;
}
