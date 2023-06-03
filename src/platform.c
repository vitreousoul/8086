#include "platform.h"

static buffer *AllocateBuffer(s32 Size)
{
    buffer *Buffer = malloc(BUFFER_ALLOC_SIZE(Size));
    Buffer->Size = Size;
    Buffer->Data = (u8 *)(Buffer + sizeof(buffer));
    return Buffer;
}

static void FreeBuffer(buffer *Buffer)
{
    free(Buffer);
}

static buffer *ReadFileIntoBuffer(char *FilePath)
{
    s32 FileSize;
    buffer *Buffer;
    FILE *File = fopen(FilePath, "rb");
    if(!File)
    {
        printf("File not found\n");
        return 0;
    }
    fseek(File, 0, SEEK_END);
    FileSize = ftell(File);
    fseek(File, 0, SEEK_SET);
    Buffer = AllocateBuffer(FileSize);
    fread(Buffer->Data, FileSize, 1, File);
    fclose(File);
    return Buffer;
}
