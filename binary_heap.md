# Binary heap structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

A binary heap of objects.

## Defining a Type
Use the macro `shlDeclareBinaryHeap` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the binary heap elements. |

Use the macro `shlDefineBinaryHeap` to generate the function implementations.

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the binary heap elements. |

```c
#include "binary_heap.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareList(IntHeap, int)
shlDefineList(IntHeap, int)
```

The list structure allows the following operations (all functions all prefixed with _typeName_):

| Function | Description | Return type |
| --- | --- | --- |
| `Init`(_typeName_* heap, _typeName_ Options options) | Initializes the data needed for the binary heap. | void |
| `Free`(_typeName_* heap) | Frees the data used by the binary heap. It doesn't free the binary heap itself. | void |
| `Push`(_typeName_* heap, _itemType_ value) | Push an element to the heap. | void |
| `Peek`(_typeName_* heap) | Gets the top of the heap without removing it. | _itemType_ |
| `Pop`(_typeName_* heap) | Remove the top of the heap. | _itemType_ | 
| `IndexOf`(_typeName_* heap, _itemType_ value) | Returns the index of an element on the heap | int32_t |
| `Contains`(_typeName_* heap, _itemType_ value) | Return `true` if an object is contained in the heap. | bool |
| `Update`(_typeName_* heap, int32_t index, _itemType_ newValue) | Update the value associated to the element at the `index` position. This may cause reordering the heap. | void |
| `Clear`(_typeName_* heap) | Clear the heap, freeing every element if a `freeFn` was provided. Doesn't free the heap itself. | void |

## Options

Each definition of a binary heap declare a struct _typeName_ Options that is used to initialize the binary heap. The struct has the following members:

| Name | Type | Description |
| --- | --- | --- |
| `compareFn` | int32_t (*)(const _itemType_, const _itemType_) | A pointer to a function that takes two elements, and returns a value `> 0` if the first element is greater than the second, returns a value `< 0` if the first element is less than the second, and returns a value `= 0` if the two elements are equal. |
| `equalsFn` | bool (*)(const _itemType_, const _itemType_) | _(optional)_ A pointer to a function that takes two elements, and returns `true` if the elements are equals, and returns `false` otherwise. If no `equalsFn` is provided then the operations `IndexOf` always returns -1 and `Contains` always return `false`. |
| `freeFn` | void (*)(_itemType_) | _(optional)_ A pointer to a function that takes an element and free it. If no `freeFn` is provided, then the operation `Clear` and `Free` doesn't free the elements and the user of the binary heap is the responsible for free the elements. |
| `defaultValue` | _itemType_ | The value to return when you apply the `Pop` operation and the binary heap is empty. |

Example:
```c
#include <stdio.h>

#include "binary_heap.h"

int32_t intCompare(const int x, const int y)
{
    return x - y;
}

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareBinaryHeap(IntHeap, int)
shlDefineBinaryHeap(IntHeap, int)

int main()
{
    IntHeapOptions options = (IntHeapOptions){0};
    options.defaultValue = 0;
    options.compareFn = intCompare;
    options.equalsFn = intEquals;

    IntHeap heap;
    IntHeapInit(&heap, options);

    for (int i = 100; i >= 0; i--)
        IntHeapPush(&heap, i);

    // print all the numbers in the heap in increasing order
    while (heap.count > 0)
    {
        int value = IntHeapPop(&heap);
        printf("%d\n", value);
    }

    return 0;
}
```