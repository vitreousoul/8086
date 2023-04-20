#define BUFFER_ALLOC_SIZE(size) (sizeof(buffer) + (size))

typedef struct
{
    s32 Size;
    u8 *Data;
} buffer;
