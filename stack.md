# Stack structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

A simple stack of objects. Internally it is implemented as an array, so Push is O(n). Pop is O(1).

## Defining a Type
Use the macro `shlDeclareStack` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| itemType | The type of the list elements. |

Use the macro `shlDefineStack` to generate the function implementations.

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| itemType | The type of the list elements. |
| equalsFn | A function that takes two elements, and returns `true` if the elementos are equals, `false` otherwise. |
| defaultValue | The value to return when you try to access an element that doesn't exist. |

```c
#include "stack.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareStack(IntStack, int)
shlDefineStack(IntStack, int, intEquals, 0)
```

This list allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| void _typeName_ Init(_typeName_ *stack) | Initializes the data needed for the stack. | void |
| void _typeName_ Free(_typeName_ *stack) | Frees the data used by the stack. It doesn't free the stack itself. | void |
| void _typeName_ Push(_typeName_ *stack, _itemType_ value) | Push an element in the top of the stack. | void |
| _itemType_ _typeName_ Peek(_typeName_ *stack) | Gets the top of the stack without removing it. | _itemType_ |
| _itemType_ _typeName_ Pop(_typeName_ *stack) | Remove the top of the stack. | _itemType _ | 
| bool _typeName_ Contains(_typeName_ *stack, _itemType_ value) | Return `true` if an object is contained in the stack. | bool | 


Example:
```c
#include <stdio.h>

#include "stack.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareStack(IntStack, int)
shlDefineStack(IntStack, int, intEquals, 0)

int main()
{
    IntStack stack;
    IntStackInit(&stack);

    for (int i = 0; i < 100; i++)
        IntStackPush(&stack, i);

    int sum = 0;

    // sum all the numbers in the stack
    while (stack.count > 0)
    {
        int value = IntStackPop(&stack);
        sum += value;
    }

    printf("The sum of the elements of the stack is %d\n", sum);

    return 0;
}
```