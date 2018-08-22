# Map structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

Represents a strongly typed collection of key-value.

## Defining a Type
Use the macro `shlDeclareMap` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| keyType | The type of the key. |
| valueType | The type of the value. |

Use the macro `shlDefineMap` to generate the function implementations.

| Argument | Description |
| --- | --- |
| typeName | The name of the generated type. This will also prefix all of the function names. |
| keyType | The type of the key. |
| valueType | The type of the value. |
| hashFn | A function that takes a key and returns a hash value for that key. |
| equalsFn | A function that takes two elements, and returns `true` if the elementos are equals, `false` otherwise. |
| defaultValue | The value to return when you try to access an element that doesn't exist. |

```c
#include "map.h"

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

shlDeclareMap(SLengthMap, const char*, int)
shlDefineMap(SLengthMap, const char*, int, fnv32, equalsStr, NULL)
```

This list allows the following operations:

| Function | Description | Return type |
| --- | --- | --- |
| void _typeName_ Init(_typeName_* map) | Initializes the data needed for the map. | void |
| void _typeName_ Free(_typeName_* map) | Frees the data used by the map. It doesn't free the map itself. | void |
| _itemType_ _typeName_ Set(_typeName_* map, _keyType_ key, _valueType_ value) | Sets the value `value` asociated with the key `key`. If the key doesn't exists, the map create it. If the key already exists, the value is replaced. This function returns the element that was previously associated with the key, or the _defaultValue_ if there were no value asociated with the key. | _itemType_ |
| _valueType_ _typeName_ Get(_typeName_* map, _keyType_ key) | Gets the value asociated with the key `key`, or _defaultValue_ if there are no value asociated with the key. | _itemType_ |
| _valueType__ _typeName_ Remove(_typeName_* map, _keyType_ key) | Remove the key `key` from the map. Returns the value asociated to the key if there were one. | _valueType_ |

Example:
```c
#include <stdio.h>
#include <string.h>

#include "map.h"

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

shlDeclareMap(SLengthMap, const char*, int)
shlDefineMap(SLengthMap, const char*, int, fnv32, equalsStr, NULL)

int main()
{
    SLengthMap map;
    SLengthMapInit(&map);

    char *strings[9] = 
    { 
        "tvnccxxgqofssyaikgij", "dehnxzqjek", "mmtlycyoosuieqw",
        "tvnccxxgqogij", "defssyaikhnxzqjek", "mmtlycyoosuieqw",
        "tvncccyoosuixxgqofssyaikgij", "dehn", "mxzqjekmtlyeqwdfsdf"
    }

    for(int i = 0; i < 9; i++)
        SLengthMapSet(&map, strings[i], strlen(strings[i]));

    printf("The length of %s is %d\n", strings[2], SLengthMapGet(&map, strings[2]));
    
    return 0;
}
```