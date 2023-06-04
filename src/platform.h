#define BUFFER_ALLOC_SIZE(size) (sizeof(buffer) + (size))

typedef struct
{
    s32 Size;
    s32 Index;
    u8 *Data;
} buffer;
