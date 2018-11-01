# List structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

Represents a strongly typed list of objects that can be accessed by index. Provides methods to search, sort, and manipulate lists.

## Defining a Type
Use the macro `shlDeclareList` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the list elements. |

Use the macro `shlDefineList` to generate the function implementations.

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the list elements. |

```c
#include "list.h"

shlDeclareList(IntList, int)
shlDefineList(IntList, int)
```

The list structure allows the following operations (all functions all prefixed with _typeName_):

| Function | Description | Return type |
| --- | --- | --- |
| `Init`(_typeName_* list, _typeName_ Options options) | Initializes the data needed for the list. | void |
| `Free`(_typeName_* list) | Frees the data used by the list. It doesn't free the list itself. | void |
| `Add`(_typeName_* list, _itemType_ value) | Add an element at the end of the list. | void |
| `AddRange`(_typeName_* list, int32_t count, itemType values[]) | Add a collection of elements at the end of the list. | void |
| `Insert`(_typeName_* list, int32_t index, itemType value) | Insert an element at the `index` position in the list. | void |
| `InsertRange`(_typeName_* list, int32_t index, int32_t count, itemType values[]) | Insert a collection of elements at the `index` position in the list. | void |
| `IndexOf`(_typeName_* list, _itemType_ value) | Gets the index of the first occurrence in the list of an element. | int32_t |
| `Get`(_typeName_* list, int32_t index) | Gets the value in the list at the `index` position. This function does check bounds of the list. If you don't want the list to check bounds when accesing elements you can use directly `list->items[index]`. | _typeName_ |
| `Set`(_typeName_* list, int32_t index, _itemType_ value) | Sets the value in the list at the `index` position. This function does check bounds of the list. If you don't want the list to check bounds when accesing elements you can assign directly `list->items[index] = value`. This function returns the element previously in the `index` position. | _typeName_ |
| `Contains`(_typeName_* list, _itemType_ value) | Return `true` if an object is contained in the list. | bool |
| `Remove`(_typeName_* list, _itemType_ value) | Remove the first occurrence of an element in the list. This function shift all the remaining elements on index to the left. | void |
| `RemoveAt`(_typeName_* list, int32_t index) | Remove the element at the position `index`. This function shift all the remaining elements on index to the left. | void |
| `RemoveAtRange`(_typeName_* list, int32_t index, int32_t count) | Remove `count` elements from the position `index`. This function shift all the remaining elements on index to the left. | void |
| `Clear`(_typeName_* list) | Clear the list, freeing every element if a `freeFn` was provided. Doesn't free the list itself. | void |
| `Reverse`(_typeName_* list) | Reverse the list. | void |
| `Sort`(_typeName_* list, int32_t (*compareFn)(const _itemType_ item1, const _itemType_ item2)) | Sort the list using the comparing function `compareFn`. This function must receive two elements `item1` and `item2` from the list and must return a value `< 0` if `item1 < item2`, a value `> 0` if `item1 > item2` and a value `= 0` if `item1 == item2` | void |
| `CopyTo`(_typeName_* list, _itemType_ array[], int32_t index) | Copy the elements of the list to `array` from the `index` position. The caller should make sure that array is big enough to fit the entire list. | void |
| `ToArray`(_typeName_* list) | Returns an array with all the elements of the list. | _itemType_* |

## Options

Each definition of a list declare a struct _typeName_ Options that is used to initialize the list. The struct has the following members:

| Name | Type | Description |
| --- | --- | --- |
| `equalsFn` | bool (*)(const _itemType_, const _itemType_) | _(optional)_ A pointer to a function that takes two elements, and returns `true` if the elements are equals, and returns `false` otherwise. If no `equalsFn` is provided then the operations `IndexOf` always return `-1`, `Contains` always return `false` and `Remove` doesn't do anything. |
| `freeFn` | void (*)(_itemType_) | _(optional)_ A pointer to a function that takes an element and free it. If no `freeFn` is provided, then the operations `Remove`, `RemoveAt`, `RemoveAtRange`, `Clear` and `Free` doesn't free the elements and the user of the list is the responsible for free the elements. |
| `defaultValue` | _itemType_ | The value to return when you try to access an element that doesn't exist. |

Example:
```c
#include <stdio.h>

#include "list.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int)

int main()
{
    IntListOptions options = (IntListOptions){0};
    options.defaultValue = 0;
    options.equalsFn = intEquals;

    IntList list;
    IntListInit(&list, options);

    for (int i = 0; i < 100; i++)
        IntListAdd(&list, i);

    // squares all the values in the list
    for (int i = 0; i < list.count; i++)
    {
        int v = IntListGet(&list, i);
        IntListSet(&list, i, v * v);
    }

    for (int i = 0; i < list.count; i++)
        printf("Element at index %d is %d\n", i, IntListGet(&list, i));

    return 0;
}
```