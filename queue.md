# Queue structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

A simple queue of objects. Internally it is implemented as a circular buffer, so Push is O(n). Pop is O(1).

## Defining a Type
Use the macro `shlDeclareQueue` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the list elements. |

Use the macro `shlDefineQueue` to generate the function implementations.

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the list elements. |

```c
#include "queue.h"

shlDeclareQueue(IntList, int)
shlDefineQueue(IntList, int)
```

This list allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| `Init`(_typeName_* queue, _typeName_ Options options) | Initializes the data needed for the queue. | void |
| `Free`(_typeName_* queue) | Frees the data used by the queue. It doesn't free the queue itself. | void |
| `Push`(_typeName_* queue, _itemType_ value) | Push an element in the top of the queue. | void |
| `Peek`(_typeName_* queue) | Gets the top of the queue without removing it. | _itemType_ |
| `Pop`(_typeName_* queue) | Remove the top of the queue. | _itemType _ | 
| `Contains`(_typeName_* queue, _itemType_ value) | Return `true` if an object is contained in the queue. | bool |
| `Clear`(_typeName_* queue) | Clear the queue, freeing every element if a `freeFn` was provided. Doesn't free the queue itself. | void |

## Options

Each definition of a queue declare a struct _typeName_ Options that is used to initialize the queue. The struct has the following members:

| Name | Type | Description |
| --- | --- | --- |
| `equalsFn` | bool (*)(const _itemType_, const _itemType_) | _(optional)_ A pointer to a function that takes two elements, and returns `true` if the elements are equals, and returns `false` otherwise. If no `equalsFn` is provided then the operation `Contains` always return `false`. |
| `freeFn` | void (*)(_itemType_) | _(optional)_ A pointer to a function that takes an element and free it. If no `freeFn` is provided, then the operation `Clear` and `Free` doesn't free the elements and the user of the queue is the responsible for free the elements. |
| `defaultValue` | _itemType_ | The value to return when you apply the `Pop` operation and the queue is empty. |

Example:
```c
#include <stdio.h>

#include "queue.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareQueue(IntQueue, int)
shlDefineQueue(IntQueue, int)

int main()
{
    IntQueueOptions options = (IntQueueOptions){0};
    options.defaultValue = 0;
    options.equalsFn = intEquals;

    IntQueue queue;
    IntQueueInit(&queue, options);

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