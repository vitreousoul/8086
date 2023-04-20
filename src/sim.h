#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;

typedef int32_t s32;

typedef size_t size;

#define BUFFER_ALLOC_SIZE(size) (sizeof(buffer) + (size))

typedef struct
{
    s32 Size;
    u8 *Data;
} buffer;
