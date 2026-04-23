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
bool mb_readUIntVar(memory_buffer_t* buffer, uint32_t* value)
{
    uint32_t v = 0;
    uint8_t byte;
    for(int32_t i = 0; i < 4; ++i)
    {
        if(!mb_read(buffer, &byte))
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
bool mb_writeUIntVar(memory_buffer_t* buffer, uint32_t value)
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
        if(!mb_write(buffer, byte))
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
    memory_buffer_t bufInput = {0};
    mb_initFromMemory(&bufInput, xmiData, xmiLength);

    memory_buffer_t bufOutput = {0};
    mb_initEmpty(&bufOutput);

    if (!mb_scanTo(&bufInput, "EVNT", 4))
    {
        mb_free(&bufOutput);
        return NULL;
    }

    if (!mb_skip(&bufInput, 8))
    {
        mb_free(&bufOutput);
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
            if (!mb_read(&bufInput, &tokenType))
            {
                mb_free(&bufOutput);
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
                if (!mb_read(&bufInput, &token->data))
                {
                    mb_free(&bufOutput);
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
                if (!mb_read(&bufInput, &token->data))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }

                if (!mb_skip(&bufInput, 1))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }

                break;
            }

            case 0x90:
            {
                if (!mb_read(&bufInput, &extendedType))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }

                token->data = extendedType;

                if (!mb_skip(&bufInput, 1))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }

                assert(mb_readUIntVar(&bufInput, &intVar));
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
                    if (!mb_read(&bufInput, &extendedType))
                    {
                        mb_free(&bufOutput);
                        return NULL;
                    }

                    if (extendedType == 0x2F)
                        end = true;
                    else if (extendedType == 0x51)
                    {
                        if (!tempoSet)
                        {
                            assert(mb_skip(&bufInput, 1));
                            assert(mb_readInt24BE(&bufInput, &tempo));
                            tempo *= 3;
                            tempoSet = true;
                            assert(mb_skip(&bufInput, -4));
                        }
                        else
                        {
                            MidiTokenListRemoveAt(&lstTokens, lstTokens.count - 1);
                            assert(mb_readUIntVar(&bufInput, &intVar));
                            if (!mb_skip(&bufInput, intVar))
                            {
                                mb_free(&bufOutput);
                                return NULL;
                            }
                            break;
                        }
                    }
                }

                token->data = extendedType;
                assert(mb_readUIntVar(&bufInput, &token->bufferLength));
                token->buffer = bufInput._pointer;

                if (!mb_skip(&bufInput, token->bufferLength))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }

                break;
            }
        }
    }

    if (lstTokens.count == 0)
    {
        mb_free(&bufOutput);
        return NULL;
    }
    if (!mb_writeString(&bufOutput, "MThd\0\0\0\x06\0\0\0\x01", 12))
    {
        mb_free(&bufOutput);
        return NULL;
    }
    if (!mb_writeUInt16BE(&bufOutput, (tempo * 3) / 25000))
    {
        mb_free(&bufOutput);
        return NULL;
    }
    if (!mb_writeString(&bufOutput, "MTrk\xBA\xAD\xF0\x0D", 8))
    {
        mb_free(&bufOutput);
        return NULL;
    }

    MidiTokenListSort(&lstTokens, compareTokens);

    tokenTime = 0;
    tokenType = 0;
    end = false;

    for (int32_t i = 0; i < lstTokens.count && !end; i++)
    {
        MidiToken t = lstTokens.items[i];

        if (!mb_writeUIntVar(&bufOutput, t.time - tokenTime))
        {
            mb_free(&bufOutput);
            return NULL;
        }

        tokenTime = t.time;

        if (t.type >= 0xF0)
        {
            tokenType = t.type;
            if (!mb_write(&bufOutput, tokenType))
            {
                mb_free(&bufOutput);
                return NULL;
            }

            if (tokenType == 0xFF)
            {
                if (!mb_write(&bufOutput, t.data))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }

                if (t.data == 0x2F)
                    end = true;
            }

            if (!mb_writeUIntVar(&bufOutput, t.bufferLength))
            {
                mb_free(&bufOutput);
                return NULL;
            }
            if (!mb_writeBytes(&bufOutput, t.buffer, t.bufferLength))
            {
                mb_free(&bufOutput);
                return NULL;
            }
        }
        else
        {
            if (t.type != tokenType)
            {
                tokenType = t.type;

                if (!mb_write(&bufOutput, tokenType))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }
            }

            if (!mb_write(&bufOutput, t.data))
            {
                mb_free(&bufOutput);
                return NULL;
            }

            if (t.buffer)
            {
                if (!mb_writeBytes(&bufOutput, t.buffer, 1))
                {
                    mb_free(&bufOutput);
                    return NULL;
                }
            }
        }
    }

    size_t length = mb_position(&bufOutput) - 22;
    assert(mb_seek(&bufOutput, 18));
    assert(mb_writeUInt32BE(&bufOutput, length));

    uint8_t* midData = mb_data(&bufOutput, midLength);

    mb_free(&bufOutput);
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
