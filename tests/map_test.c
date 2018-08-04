#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../map.h"

#define FNV_PRIME_32 0x01000193
#define FNV_OFFSET_32 0x811c9dc5

uint32_t fnv32(const char* data) {
    uint32_t hash = FNV_OFFSET_32;
    while(*data != 0)
        hash = (*data++ ^ hash) * FNV_PRIME_32;

    return hash;
}

shlDeclareMap(SSMap, char*, char*)
shlDefineMap(SSMap, char*, char*, fnv32, strcmp, NULL)

uint32_t intHash(const uint32_t x)
{
    return x;
}

int cmprInt(const uint32_t a, const uint32_t b)
{
    return a < b ? -1 : (a > b ? 1 : 0);
}

shlDeclareMap(IntMap, uint32_t, uint32_t)
shlDefineMap(IntMap, uint32_t, uint32_t, intHash, cmprInt, 0)

char *strings[] =
{
    "tvnccxxgqo",
    "fssyaikgij",
    "dehnxzqjek",
    "mmtlycyoos",
    "uieqwwqnxg",
    "zqqaifqaai",
    "nataxfclgc",
    "lpqksqcyjs",
    "jxzbmltzhe",
    "nbegmljgbf",
    "pabugplwwk",
    "nophwmwkcd",
    "vhttftmhno",
    "wqsnhpxzcb",
    "ajfvkpcrrf",
    "wkoyyehqkn",
    "vhfrjzkrcr",
    "jrhyulcbbd",
    "xoneyrakfc",
    "mbgnqwjnhy",
    "ryclbmddzb",
    "sraiyehhdo",
    "zqjznjmpcb",
    "fvopqdoglz",
    "fhekhnzqrc",
    "yqriapjuof",
    "moybhieaim",
    "gvewlhatel",
    "kdkdwmuoct",
    "ujqxagumfw",
    "khakhplrcs",
    "eqgmauxywq",
    "qeozrptxtz",
    "gaeaifvqhk",
    "ttendndgkr",
    "avaxeshimd",
    "fsaruycfhl",
    "gfuytoqsrp",
    "msphdbwvsw",
    "jrqoyxgtaf",
    "qzdelxaexh",
    "okwrycylrh",
    "hzfecantgt",
    "rrjlkgnrbq",
    "ccxqqtpszj",
    "phufpxfqum",
    "idsgyiupnz",
    "ixikhqqome",
    "vmwimuduig",
    "tqaxnbfqvb",
    "ihpdwxqvjf",
    "jwwpuopulw",
    "uhztpeabix",
    "wdnjdfuuco",
    "siwbslsbbt",
    "qprkfnycez",
    "cciwkkpthd",
    "afcszzjyhu",
    "cmdtlxzeeq",
    "gwfnbfxxau",
    "saifgbqnhq",
    "kdssfegewf",
    "amrgbgbcpi",
    "zvrbvsfqpw",
    "zzcomcwbxr",
    "svhtimtvbk",
    "zawimzhkkz",
    "acjhsuiies",
    "qazhmcfbih",
    "fsetobdlct",
    "qvbsizfonr",
    "djxjuptbwu",
    "hmaeicjkpm",
    "bmxhtfqeph",
    "axehwgnpcw",
    "vavqwzhvbw",
    "luvdbodgno",
    "bzpawyoekp",
    "lcokdmhscn",
    "vfloujsdsl",
    "glkphcpcev",
    "awznihkqrt",
    "bahwpxriyl",
    "rpzurgmjrm",
    "auxtsbsqzo",
    "adbsmazfqk",
    "hkdeoimqyl",
    "joyrsatvoo",
    "znbsfdrkzh",
    "uxtmuxcsdm",
    "ypporrsyhx",
    "ddyhhjbitx",
    "glzxciezsf",
    "cbevvojzlr",
    "axvmktlhjy",
    "srbcgiozhh",
    "fablsjwcfj",
    "imyboezsof",
    "mepkghdipl",
    "skcpltogsd",
};

char *toupper_string(char *str)
{
    int len = strlen(str);
    char *upper = (char*)calloc(len + 1, sizeof(char));
    
    for(int i = 0; i < len; i++)
    {
        upper[i] = toupper(str[i]);
    }
    
    return upper;
}

int main(int argc, char **argv)
{
    printf("Testing string map\n");
    printf("---------------------\n");
    printf("---------------------\n");

    SSMap map;
    SSMapInit(&map);

    for(int i = 0; i < 100; i++)
    {
        SSMapSet(&map, strings[i], toupper_string(strings[i]));
    }
    
    for(int i = 40; i <= 80; i++)
    {
        printf("Map value for '%s' is '%s'\n", strings[i], SSMapGet(&map, strings[i]));
    }

    SSMapSet(&map, strings[56], "hello world!");
    printf("Map value for '%s' is '%s'\n", strings[56], SSMapGet(&map, strings[56]));

    printf("Contains 56 = %d\n", SSMapContains(&map, strings[56]));
    SSMapRemove(&map, strings[56]);
    printf("Contains 56 = %d\n", SSMapContains(&map, strings[56]));

    printf("Map value for '%s' is '%s'\n", strings[56], SSMapGet(&map, strings[56]));

    printf("Testing int map\n");
    printf("---------------------\n");
    printf("---------------------\n");

    IntMap map2;
    IntMapInit(&map2);
    
    for(int i = 0; i < 100; i++)
    {   
        IntMapSet(&map2, i, i * i);
    }
    
    for(int i = 20; i <= 40; i++)
    {
        printf("Map value for '%d' is '%d'\n", i, IntMapGet(&map2, i));        
    }

    printf("Map value for '%d' is '%d'\n", 101, IntMapGet(&map2, 101));        
    
    return 0;
}