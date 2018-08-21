# List structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

Represents a strongly typed list of objects that can be accessed by index. Provides methods to search, sort, and manipulate lists.

## Defining a Type
In a header file, use the macro shlDeclareList to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| itemType | The type of the list elements. |

In the corresponding source file, use the macro shlDefineList to generate the function implementations.

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| itemType | The type of the list elements. |
| equalsFn | A function that takes two elements, and returns `true` if the elementos are equals, `false` otherwise. |
| defaultValue | The value to return when you try to access an element that doesn't exist. |

```
#include "list.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int, intEquals, 0)
```

This list allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| void _typeName_ Init(_typeName_ *list) | Initializes the data needed for the list. | void |
| void _typeName_ Free(_typeName_ *list) | Frees the data used by the list. It doesn't free the list itself. | void |
| void _typeName_ Add(_typeName_ *list, _itemType_ value) | Add an element at the end of the list. | void |
| void _typeName_ Insert(_typeName_ *list, int32_t index, itemType value) | Insert an element at the `index` position in the list. | void |
| int32_t _typeName_ IndexOf(_typeName_ *list, _itemType_ value) | Gets the index of the first occurrence in the list of an element. | int32_t |
| _itemType_ _typeName_ Get(_typeName_ *list, int32_t index) | Gets the value in the list at the `index` position. This function does check bounds of the list. If you don't want the list to check bounds when accesing elements you can use directly `list->items[index]'. | _typeName_ |
| bool _typeName_ Contains(_typeName_ *list, _itemType_ value) | Return `true` if an object is contained in the list. | bool |
| void _typeName_ Clear(_typeName_ *list) | Clear the list. This doesn't free the list, it merely reset the count to zero. | void |
| _itemType_ _typeName_ RemoveAt(_typeName_ *list, int32_t index) | Remove the element at the position `index`. This function shift all the remaining elements on index to the left. This function returns the element removed. | _itemType_ |
| _itemType_ _typeName_ Remove(_typeName_ *list, _itemType_ value) | Remove the first occurrence of an element in the list. This function shift all the remaining elements on index to the left. This function returns the element removed. | _itemType_ |

Example:
```
#include "list.h"

bool intEquals(const int x, const int y)
{
    return x == y;
}

shlDeclareList(IntList, int)
shlDefineList(IntList, int, intEquals, 0)

int main()
{
    IntList list;
    IntListInit(&list);

    for (int i = 0; i < 100; i++)
        IntListAdd(&list, i);

    // squares all the values in the list
    for (int i = 0; i < 100; i++)
    {
        int v = IntListGet(&list, i);
        IntListSet(&list, i, v * v);
    }

    for (int i = 0; i < list.count; i++)
        printf("Element at index %d is %d\n", i, IntListGet(&list, i));

    return 0;
}
```