# Queue structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

A simple queue of objects. Internally it is implemented as a circular buffer, so Push is O(n). Pop is O(1).

## Defining a Type
Use the macro `shlDeclareQueue` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| itemType | The type of the list elements. |

Use the macro `shlDefineQueue` to generate the function implementations.

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| itemType | The type of the list elements. |
| equalsFn | A function that takes two elements, and returns `true` if the elementos are equals, `false` otherwise. |
| defaultValue | The value to return when you try to access an element that doesn't exist. |

```c
#include "queue.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareQueue(IntList, int)
shlDefineQueue(IntList, int, intEquals, 0)
```

This list allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| void _typeName_ Init(_typeName_ *queue) | Initializes the data needed for the queue. | void |
| void _typeName_ Free(_typeName_ *queue) | Frees the data used by the queue. It doesn't free the queue itself. | void |
| void _typeName_ Push(_typeName_ *queue, _itemType_ value) | Push an element in the top of the queue. | void |
| _itemType_ _typeName_ Peek(_typeName_ *queue) | Gets the top of the queue without removing it. | _itemType_ |
| _itemType_ _typeName_ Pop(_typeName_ *queue) | Remove the top of the queue. | _itemType _ | 
| bool _typeName_ Contains(_typeName_ *queue, _itemType_ value) | Return `true` if an object is contained in the queue. | bool | 

Example:
```c
#include "queue.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareQueue(IntQueue, int)
shlDefineQueue(IntQueue, int, intEquals, 0)

int main()
{
    IntQueue queue;
    IntQueueInit(&queue);

    for (int i = 0; i < 100; i++)
        IntQueuePush(&queue, i);

    int sum = 0;

    // sum all the numbers in the queue
    while (queue.count > 0)
    {
        int value = IntQueuePop(&queue);
        sum += value;
    }

    printf("The sum of the elements of the queue is %d\n", sum);

    return 0;
}
```