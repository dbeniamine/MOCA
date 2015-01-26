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

int main()
{
    //Init
    int i, nbAdd, *ptr,st, **keys,j;// k;
    foo e;
    hash_map map=Moca_InitHashMap(14,2*(1UL<<14), sizeof(struct _foo),NULL);
    srand(time(NULL));

    nbAdd=1000;
    keys=calloc(nbAdd, sizeof(int *));
    //for(k=0;k<10;++k)
    //{
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
        e= (foo)Moca_AddToMap(map,ptr, &st);
        /* printf("inserting %p, %d/%d res: %p / %d\n", ptr, i, nbAdd, e, st); */
        if(e)
        {
            e->val=rand()%10;
            e->other_val=rand()%10;
        }
        printf("inserted e %p, k %p, n: %d, v %d, ov %d\n", e, e->key,
                e->next,e->val, e->other_val);
        printf("%d elements in map\n", Moca_NbElementInMap(map));
    }
    for(i=0;i<nbAdd;i++)
    {
        j=Moca_PosInMap(map, keys[i]);
        printf("key %p at pos %d\n",keys[i],j);
        e=(foo)Moca_EntryAtPos(map,(unsigned)j);
        printf("Entry at pos %d, %p %p %d %d %d\n", j,e, e->key, e->next,
                e->val, e->other_val);
    }
    printf("All insert done, table size :%d\n", Moca_NbElementInMap(map));
    for(i=0;i<nbAdd/2;i++)
    {
        e=(foo)Moca_RemoveFromMap(map, keys[i]);
        printf("Removing key %p %p %p %d %d %d\n", keys[i], e,
                e->key, e->next, e->val,e->other_val);
        printf("Still %d elements \n", Moca_NbElementInMap(map));
        free(e->key);
        keys[i]=NULL;
    }
    printf("half remove done, table size :%d\n", Moca_NbElementInMap(map));
    i=0;
    while((e=(foo)Moca_NextEntryPos(map, (unsigned int *)&i)))
    {
        printf("Entry at pos %d, %p %p %d %d %d\n", i-1,e, e->key, e->next,
                e->val, e->other_val);
        Moca_RemoveFromMap(map,e->key);
    }
    printf("All remove done, table size :%d\n", Moca_NbElementInMap(map));
    //}
    Moca_FreeMap(map);
    return 0;
}
