#include <stdio.h>
#include <stdlib.h>

#include "../list.h"

bool intEquals(int x, int y)
{
    return x == y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int, intEquals, 0)

int main(int argc, char **argv)
{
    IntList list;
    IntListInit(&list);

    for(int i = 0; i < 10; i++)
    {
        IntListAdd(&list, i);
    }
    
    for(int i = 0; i < list.count; i++)
    {
        printf("Element at position %d is %d\n", i, IntListGet(&list, i));
    }
    
    printf("----------\n");
    IntListRemoveByIndex(&list, 3);

    for(int i = 0; i < list.count; i++)
    {
        printf("Element at position %d is %d\n", i, IntListGet(&list, i));
    }

    printf("----------\n");
    IntListRemoveByValue(&list, 7);

    for(int i = 0; i < list.count; i++)
    {
        printf("Element at position %d is %d\n", i, IntListGet(&list, i));
    }

    printf("----------\n");
    IntListInsert(&list, 0, 10);

    for(int i = 0; i < list.count; i++)
    {
        printf("Element at position %d is %d\n", i, IntListGet(&list, i));
    }

    return 0;
}