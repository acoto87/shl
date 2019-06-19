#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "../memory_buffer.h"

#include "../list.h"

/**
 * Extension function of MemoryBuffer to read variable lengths integer values.
 */
bool mbReadUIntVar(MemoryBuffer* buffer, uint32_t* value)
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

/**
 * Extension function of MemoryBuffer to write variable lengths integer values.
 */
bool mbWriteUIntVar(MemoryBuffer* buffer, uint32_t value)
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
    int32_t time;
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

/**
 * This code is a port in C of the XMI2MID converter by Peter "Corsix" Cawley 
 * in the War1gus repository. You can find the original C++ code here: 
 * https://github.com/Wargus/war1gus/blob/master/xmi2mid.cpp.
 * 
 * To understand more about these formats see:
 * http://www.shikadi.net/moddingwiki/XMI_Format
 * http://www.shikadi.net/moddingwiki/MID_Format
 * https://github.com/colxi/midi-parser-js/wiki/MIDI-File-Format-Specifications
 */
uint8_t* transcodeXmiToMid(uint8_t* xmiData, size_t xmiLength, size_t* midLength)
{
    MemoryBuffer bufInput = {0};
    mbInitFromMemory(&bufInput, xmiData, xmiLength);

    MemoryBuffer bufOutput = {0};
    mbInitEmpty(&bufOutput);

    if (!mbScanTo(&bufInput, "EVNT", 4))
    {
        mbFree(&bufOutput);
        return NULL;
    }

    if (!mbSkip(&bufInput, 8))
    {
        mbFree(&bufOutput);
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
                mbFree(&bufOutput);
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
                    mbFree(&bufOutput);
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
                    mbFree(&bufOutput);
                    return NULL;
                }

                if (!mbSkip(&bufInput, 1))
                {
                    mbFree(&bufOutput);
                    return NULL;
                }

                break;
            }

            case 0x90:
            {
                if (!mbRead(&bufInput, &extendedType))
                {
                    mbFree(&bufOutput);
                    return NULL;
                }

                token->data = extendedType;

                if (!mbSkip(&bufInput, 1))
                {
                    mbFree(&bufOutput);
                    return NULL;
                }

                assert(mbReadUIntVar(&bufInput, &intVar));
                token = MidiTokenListAppend(&lstTokens, tokenTime + intVar * 3, tokenType);
                token->data = extendedType;
                token->buffer = (uint8_t*)"\0";
            
                break;
            }

            case 0xF0:
            {
                extendedType = 0;

                if (tokenType == 0xFF)
                {
                    if (!mbRead(&bufInput, &extendedType))
                    {
                        mbFree(&bufOutput);
                        return NULL;
                    }

                    if (extendedType == 0x2F)
                        end = true;
                    else if (extendedType == 0x51)
                    {
                        if (!tempoSet)
                        {
                            assert(mbSkip(&bufInput, 1));
                            assert(mbReadInt24BE(&bufInput, &tempo));
                            tempo *= 3;
                            tempoSet = true;
                            assert(mbSkip(&bufInput, -4));
                        }
                        else
                        {
                            MidiTokenListRemoveAt(&lstTokens, lstTokens.count - 1);
                            assert(mbReadUIntVar(&bufInput, &intVar));
                            if (!mbSkip(&bufInput, intVar))
                            {
                                mbFree(&bufOutput);
                                return NULL;
                            }
                            break;
                        }
                    }
                }

                token->data = extendedType;
                assert(mbReadUIntVar(&bufInput, &token->bufferLength));
                token->buffer = bufInput._pointer;

                if (!mbSkip(&bufInput, token->bufferLength))
                {
                    mbFree(&bufOutput);
                    return NULL;
                }

                break;
            }
        }
    }

    if (lstTokens.count == 0)
    {
        mbFree(&bufOutput);
        return NULL;
    }
    if (!mbWriteString(&bufOutput, "MThd\0\0\0\x06\0\0\0\x01", 12))
    {
        mbFree(&bufOutput);
        return NULL;
    }
    if (!mbWriteUInt16BE(&bufOutput, (tempo * 3) / 25000))
    {
        mbFree(&bufOutput);
        return NULL;
    }
    if (!mbWriteString(&bufOutput, "MTrk\xBA\xAD\xF0\x0D", 8))
    {
        mbFree(&bufOutput);
        return NULL;
    }

    MidiTokenListSort(&lstTokens, compareTokens);

    tokenTime = 0;
    tokenType = 0;
    end = false;

    for (int32_t i = 0; i < lstTokens.count && !end; i++)
    {
        MidiToken t = lstTokens.items[i];

        if (!mbWriteUIntVar(&bufOutput, t.time - tokenTime))
        {
            mbFree(&bufOutput);
            return NULL;
        }

        tokenTime = t.time;

        if (t.type >= 0xF0)
        {
            tokenType = t.type;
            if (!mbWrite(&bufOutput, tokenType))
            {
                mbFree(&bufOutput);
                return NULL;
            }

            if (tokenType == 0xFF)
            {
                if (!mbWrite(&bufOutput, t.data))
                {
                    mbFree(&bufOutput);
                    return NULL;
                }

                if (t.data == 0x2F)
                    end = true;
            }

            if (!mbWriteUIntVar(&bufOutput, t.bufferLength))
            {
                mbFree(&bufOutput);
                return NULL;
            }
            if (!mbWriteBytes(&bufOutput, t.buffer, t.bufferLength))
            {
                mbFree(&bufOutput);
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
                    mbFree(&bufOutput);
                    return NULL;
                }
            }

            if (!mbWrite(&bufOutput, t.data))
            {
                mbFree(&bufOutput);
                return NULL;
            }

            if (t.buffer)
            {
                if (!mbWriteBytes(&bufOutput, t.buffer, 1))
                {
                    mbFree(&bufOutput);
                    return NULL;
                }
            }
        }
    }

    size_t length = mbPosition(&bufOutput) - 22;
    assert(mbSeek(&bufOutput, 18));
    assert(mbWriteUInt32BE(&bufOutput, length));

    uint8_t* midData = mbGetData(&bufOutput, midLength);

    mbFree(&bufOutput);
    return midData;
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
