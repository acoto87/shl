#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"

#include "../list.h"

const char* BufferTestStr = "Buffer test";
const char* EndBlockStr = "End block";

static float getTime()
{
    return (float)clock() / CLOCKS_PER_SEC;
}

bool readUIntVar(MemoryBuffer* buffer, uint32_t* value)
{
    uint32_t v = 0;
    uint8_t byte;
    for(int32_t i = 0; i < 4; ++i)
    {
        if(!mbRead(buffer, &byte))
            return false;

        v = (v << 7) | (uint32_t)(byte & 0x7F);
        if((byte & 0x80) == 0)
            break;
    }
    *value = v;
    return true;
}

bool writeUIntVar(MemoryBuffer* buffer, uint32_t value)
{
    int32_t byteCount = 1;
    uint32_t v = value & 0x7F;
    value >>= 7;
    while (value)
    {
        v = (v << 8) | 0x80 | (value & 0x7F);
        ++byteCount;
        value >>= 7;
    }
    
    for(int32_t i = 0; i < byteCount; ++i)
    {
        uint8_t byte = v & 0xFF;
        if(!mbWrite(buffer, byte))
            return false;
        v >>= 8;
    }
    return true;
}

typedef struct _MidiToken
{
    int32_t  time;
    uint32_t bufferLength;
    uint8_t* buffer;
    uint8_t  type;
    uint8_t  data;
} MidiToken;

static int32_t compareTokens(const MidiToken left, const MidiToken right)
{
    return left.time - right.time;
}

shlDeclareList(MidiTokenList, MidiToken)
shlDefineList(MidiTokenList, MidiToken)

static MidiToken* MidiTokenListAppend(MidiTokenList* list, int32_t time, uint8_t type)
{
    MidiToken token = {0};
    token.time = time;
    token.type = type;
    MidiTokenListAdd(list, token);
    return &list->items[list->count - 1];
}

#define printNullAt() printf("NULL at %d\n", __LINE__)

uint8_t* transcodeXmiToMid(uint8_t* xmiData,
                           size_t xmiLength, size_t* midLength)
{
    MemoryBuffer bufInput = {0};
    mbInitFromMemory(&bufInput, xmiData, xmiLength);

    MemoryBuffer bufOutput = {0};
    mbInitEmpty(&bufOutput);

    if (!mbScanTo(&bufInput, "EVNT", 4))
    {
        printNullAt();
        return NULL;
    }

    if (!mbSkip(&bufInput, 8))
    {
        printNullAt();
        return NULL;
    }

    MidiTokenListOptions options = {0};

    MidiTokenList lstTokens;
    MidiTokenListInit(&lstTokens, options);

    MidiToken* token;
    int32_t tokenTime = 0;
    int32_t tempo = 500000;
    bool tempoSet = false;
    bool end = false;
    uint8_t tokenType, extendedType;
    uint32_t intVar;

    while (!mbIsEOF(&bufInput) && !end)
    {
        while (true)
        {
            if (!mbRead(&bufInput, &tokenType))
            {
                printNullAt();
                return NULL;
            }

            if (tokenType & 0x80)
                break;

            tokenTime += (int32_t)tokenType * 3;
        }

        token = MidiTokenListAppend(&lstTokens, tokenTime, tokenType);
        token->buffer = bufInput._pointer + 1;

        switch (tokenType & 0xF0)
        {
            case 0xC0:
            case 0xD0:
            {
                if (!mbRead(&bufInput, &token->data))
                {
                    printNullAt();
                    return NULL;
                }

                token->buffer = NULL;
                break;
            }

            case 0x80:
            case 0xA0:
            case 0xB0:
            case 0xE0:
            {
                if (!mbRead(&bufInput, &token->data))
                {
                    printNullAt();
                    return NULL;
                }

                if (!mbSkip(&bufInput, 1))
                {
                    printNullAt();
                    return NULL;
                }

                break;
            }

            case 0x90:
            {
                if (!mbRead(&bufInput, &extendedType))
                {
                    printNullAt();
                    return NULL;
                }

                token->data = extendedType;

                if (!mbSkip(&bufInput, 1))
                {
                    printNullAt();
                    return NULL;
                }

                readUIntVar(&bufInput, &intVar);
                token = MidiTokenListAppend(&lstTokens, tokenTime + intVar * 3, tokenType);
                token->data = extendedType;
                token->buffer = 0;
            
                break;
            }

            case 0xF0:
            {
                extendedType = 0;

                if (tokenType == 0xFF)
                {
                    if (!mbRead(&bufInput, &extendedType))
                    {
                        printNullAt();
                        return NULL;
                    }

                    if (extendedType == 0x2F)
                        end = true;
                    else if (extendedType == 0x51)
                    {
                        if (tempoSet)
                        {
                            mbSkip(&bufInput, 1);
                            mbReadInt24BE(&bufInput, &tempo);
                            tempo *= 3;
                            tempoSet = true;
                            mbSkip(&bufInput, 1);
                        }
                        else
                        {
                            MidiTokenListRemoveAt(&lstTokens, lstTokens.count - 1);
                            readUIntVar(&bufInput, &intVar);
                            if (!mbSkip(&bufInput, intVar))
                            {
                                printNullAt();
                                return NULL;
                            }
                            break;
                        }
                    }
                }

                token->data = extendedType;
                readUIntVar(&bufInput, &token->bufferLength);
                token->buffer = bufInput._pointer;

                if (!mbSkip(&bufInput, token->bufferLength))
                {
                    printNullAt();
                    return NULL;
                }

                break;
            }
        }
    }

    if (lstTokens.count == 0)
    {
        printNullAt();
        return NULL;
    }
    if (!mbWriteString(&bufOutput, "MThd\0\0\0\x06\0\0\0\x01", 12))
    {
        printNullAt();
        return NULL;
    }
    if (!mbWriteUInt16BE(&bufOutput, (tempo * 3) / 25000))
    {
        printNullAt();
        return NULL;
    }
    if (!mbWriteString(&bufOutput, "MTrk\xBA\xAD\xF0\x0D", 8))
    {
        printNullAt();
        return NULL;
    }

    MidiTokenListSort(&lstTokens, compareTokens);

    tokenTime = 0;
    tokenType = 0;
    end = false;

    for (int32_t i = 0; i < lstTokens.count; i++)
    {
        MidiToken t = lstTokens.items[i];

        if (!writeUIntVar(&bufOutput, t.time - tokenTime))
        {
            printNullAt();
            return NULL;
        }

        tokenTime = t.time;

        if (t.type >= 0xF0)
        {
            tokenType = t.type;
            if (!mbWrite(&bufOutput, tokenType))
            {
                printNullAt();
                return NULL;
            }

            if (tokenType == 0xFF)
            {
                if (!mbWrite(&bufOutput, t.data))
                {
                    printNullAt();
                    return NULL;
                }

                if (t.data == 0x2F)
                    end = true;
            }

            if (!writeUIntVar(&bufOutput, t.bufferLength))
            {
                printNullAt();
                return NULL;
            }
            if (!mbWriteBytes(&bufOutput, t.buffer, t.bufferLength))
            {
                printNullAt();
                return NULL;
            }
        }
        else
        {
            if (t.type != tokenType)
            {
                tokenType = t.type;

                if (!mbWrite(&bufOutput, tokenType))
                {
                    printNullAt();
                    return NULL;
                }
            }

            if (!mbWrite(&bufOutput, t.data))
            {
                printNullAt();
                return NULL;
            }

            if (t.buffer)
            {
                if (!mbWriteBytes(&bufOutput, t.buffer, 1))
                {
                    printNullAt();
                    return NULL;
                }
            }
        }
    }

    size_t length = mbPosition(&bufOutput) - 22;
    mbSeek(&bufOutput, 18);
    mbWriteUInt32BE(&bufOutput, length);

    return mbGetData(&bufOutput, midLength);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("Need path to .xmi file\n");
        return -1;
    }

    char* inFileName = argv[1];

    uint8_t* xmiData;
    size_t xmiLength;

    uint8_t* midData;
    size_t midLength;

    printf("%s\n", inFileName);

    FILE* f = fopen(inFileName, "rb");
    if (!f)
    {
        printf("Couldn't open .xmi file\n");
        return -1;
    }

    fseek(f, 0, SEEK_END);
    xmiLength = ftell(f);
    fseek(f, 0, SEEK_SET);
    xmiData = (uint8_t*)malloc(xmiLength);
    fread(xmiData, sizeof(uint8_t), xmiLength, f);
    fclose(f);

    printf("%d\n", xmiLength);
    printf("%s\n", xmiData);

    midData = transcodeXmiToMid(xmiData, xmiLength, &midLength);
    if (midData)
    {
        f = fopen("output.mid", "wb");
        if (!f)
        {
            printf("Couldn't open .mid file\n");
            return -1;
        }
        fwrite(midData, sizeof(uint8_t), midLength, f);
        fclose(f);
        free(midData);
    }
    else
    {
        printf("midData is NULL\n");
    }

    free(xmiData);
    return 0;
}
