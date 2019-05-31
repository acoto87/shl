# Map structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

Represents a strongly typed collection of non-repeating values with fast access via hashing.

## Defining a Type
Use the macro `shlDeclareSet` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the set elements. |

Use the macro `shlDefineSet` to generate the function implementations.

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `itemType` | The type of the set elements. |

```c
#include "set.h"

shlDeclareSet(SSet, const char*)
shlDefineSet(SSet, const char*)
```

The set structure allows the following operations (all functions all prefixed with _typeName_):

void typeName ## Init(typeName* map, typeName ## Options options); \
    void typeName ## Free(typeName* map); \
    void typeName ## Add(typeName* set, itemType item); \
    bool typeName ## Contains(typeName* set, itemType item); \
    void typeName ## Remove(typeName* set, itemType item); \
    void typeName ## Clear(typeName* set); \

| Function | Description | Return type |
| --- | --- | --- |
| `Init`(_typeName_* set, _typeName_ Options options) | Initializes the data needed for the set. | void |
| `Free`(_typeName_* set) | Frees the data used by the set. It doesn't free the set itself. | void |
| `Add`(_typeName_* set, _itemType_ item) | Add an item to the set and returns `true` if it was inserted, `false` otherwise. | bool |
| `Contains`(_typeName_* set, _itemType_ item) | Return `true` an item is contained in the set. | bool |
| `Remove`(_typeName_* set, _itemType_ item) | Remove the item `item` from the set, freeing the item if a `freeFn` function was provided. | void |
| `Clear`(_typeName_* set) | Clear the set, freeing every element if a `freeFn` was provided. Doesn't free the set itself. | void |

## Options

Each definition of a set declare a struct _typeName_ Options that is used to initialize the map. The struct has the following members:

| Name | Type | Description |
| --- | --- | --- |
| `hashFn` | uint32_t (*)(const _itemType_) | A pointer to a function that takes an item and returns a hash value for that item. |
| `equalsFn` | bool (*)(const _itemType_, const _itemType_) | A pointer to a function that takes two items, and returns `true` if the items are equals, and returns `false` otherwise. |
| `freeFn` | void (*)(_itemType_) | _(optional)_ A pointer to a function that takes an element and free it. If no `freeFn` is provided, then the operations `Remove`, `Clear` and `Free` doesn't free the elements and the user of the set is the responsible for freeing the elements. |
| `defaultValue` | _itemType_ | For the set this is an internal value used when you remove an element. |

Example:
```c
#include <stdio.h>
#include <string.h>

#include "set.h"

uint32_t fnv32(const char* data) 
{
    uint32_t hash = FNV_OFFSET_32;
    while(*data != 0)
        hash = (*data++ ^ hash) * FNV_PRIME_32;

    return hash;
}

bool equalsStr(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

shlDeclareSet(SSet, const char*)
shlDefineSet(SSet, const char*)

int main()
{
    SSetOptions options = (SSetOptions){0};
    options.hashFn = fnv32;
    options.equalsFn = equalsStr;
    options.defaultValue = NULL;

    SSet set;
    SSetInit(&set, options);

    char *strings[9] = 
    { 
        "tvnccxxgqofssyaikgij", "dehnxzqjek", "mmtlycyoosuieqw",
        "tvnccxxgqofssyaikgij", "tvnccxxgqofssyaikgij", "mmtlycyoosuieqw",
        "tvncccyoosuixxgqofssyaikgij", "dehn", "tvnccxxgqofssyaikgij"
    }

    for(int i = 0; i < 9; i++)
        SSetAdd(&map, strings[i]);

    printf("The number of items in the set is %d\n", set.count);
    
    return 0;
}
```