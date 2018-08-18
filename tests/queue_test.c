#include <stdio.h>
#include <stdlib.h>

#include "../queue.h"

bool intEquals(int x, int y)
{
    return x == y;
}

shlDeclareQueue(IntQueue, int)
shlDefineQueue(IntQueue, int, intEquals, 0)

int main(int argc, char **argv)
{
    IntQueue queue;
    IntQueueInit(&queue);

    for(int i = 0; i < 10; i++)
    {
        IntQueueEnqueue(&queue, i);
    }
    
    for(int i = 0; i < queue.count; i++)
    {
        printf("Element at position %d is %d\n", i, queue.items[(queue.head + i) % queue.count]);
    }
    
    printf("----------\n");
    int value = IntQueuePeek(&queue);
    printf("Peek value: %d\n", value);

    for(int i = 0; i < queue.count / 2; i++)
    {
        printf("Pop value: %d\n", IntQueueDequeue(&queue));
    }

    return 0;
}