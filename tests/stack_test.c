#include <stdio.h>
#include <stdlib.h>

#include "../stack.h"

bool intEquals(int x, int y)
{
    return x == y;
}

shlDeclareStack(IntStack, int)
shlDefineStack(IntStack, int, intEquals, 0)

int main(int argc, char **argv)
{
    IntStack stack;
    IntStackInit(&stack);

    for(int i = 0; i < 10; i++)
    {
        IntStackPush(&stack, i);
    }
    
    for(int i = 0; i < stack.count; i++)
    {
        printf("Element at position %d is %d\n", i, stack.items[i]);
    }
    
    printf("----------\n");
    int value = IntStackPeek(&stack);
    printf("Peek value: %d\n", value);

    for(int i = 0; i < stack.count / 2; i++)
    {
        printf("Pop value: %d\n", IntStackPop(&stack));
    }

    return 0;
}