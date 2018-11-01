# Map structure
_This project is based on [GenericMap](https://github.com/mystborn/GenericMap) by mystborn, so there similar function names and structures_

Represents a strongly typed collection of key-value.

## Defining a Type
Use the macro `shlDeclareMap` to generate the type and function definitions. It has the following arguments:

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `keyType` | The type of the key. |
| `valueType` | The type of the value. |

Use the macro `shlDefineMap` to generate the function implementations.

| Argument | Description |
| --- | --- |
| `typeName` | The name of the generated type. This will also prefix all of the function names. |
| `keyType` | The type of the key. |
| `valueType` | The type of the value. |

```c
#include "map.h"

shlDeclareMap(SLengthMap, const char*, int)
shlDefineMap(SLengthMap, const char*, int)
```

The map structure allows the following operations (all functions all prefixed with _typeName_):

| Function | Description | Return type |
| --- | --- | --- |
| `Init`(_typeName_* map, _typeName_ Options options) | Initializes the data needed for the map. | void |
| `Free`(_typeName_* map) | Frees the data used by the map. It doesn't free the map itself. | void |
| `Contains`(_typeName_* map, _keyType_ key) | Return `true` a key is contained in the map. | bool |
| `Get`(_typeName_* map, _keyType_ key) | Gets the value asociated with the key `key`, or _defaultValue_ if there are no value asociated with the key. | _valueType_ |
| `Set`(_typeName_* map, _keyType_ key, _valueType_ value) | Sets the value `value` asociated with the key `key`. If the key doesn't exists, the map create it. If the key already exists, the value is replaced, freeing the previous value if a `freeFn` function was provided.  | void |
| `Remove`(_typeName_* map, _keyType_ key) | Remove the key `key` from the map, freeing the value associated with the key if a `freeFn` function was provided.  | void |
| `Clear`(_typeName_* list) | Clear the map, freeing every element if a `freeFn` was provided. Doesn't free the map itself. | void |

## Options

Each definition of a map declare a struct _typeName_ Options that is used to initialize the map. The struct has the following members:

| Name | Type | Description |
| --- | --- | --- |
| `hashFn` | uint32_t (*)(const _keyType_) | A pointer to a function that takes a key and returns a hash value for that key. |
| `equalsFn` | bool (*)(const _keyType_, const _keyType_) | A pointer to a function that takes two keys, and returns `true` if the keys are equals, and returns `false` otherwise. |
| `freeFn` | void (*)(_valueType_) | _(optional)_ A pointer to a function that takes an element and free it. If no `freeFn` is provided, then the operations `Set` (when there is a value to replace), `Remove`, `Clear` and `Free` doesn't free the elements and the user of the map is the responsible for free the elements. |
| `defaultValue` | _valueType_ | The value to return when you try to access an element that doesn't exist. |

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
shlDefineMap(SLengthMap, const char*, int)

int main()
{
    SLengthMapOptions options = (SLengthMapOptions){0};
    options.hashFn = fnv32;
    options.equalsFn = equalsStr;
    options.defaultValue = NULL;

    SLengthMap map;
    SLengthMapInit(&map, options);

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