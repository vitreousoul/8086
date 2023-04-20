#include "sim.h"

static buffer *AllocateBuffer(Size)
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

static s32 TestSim()
{
    s32 I, Result = 0;
    buffer *Buffer = ReadFileIntoBuffer("../assets/test.txt");
    if(!Buffer)
    {
        return 1;
    }
    printf("buffer\n");
    for(I = 0; I < Buffer->Size; ++I)
    {
        printf("0x%x ", Buffer->Data[I]);
        if(Buffer->Data[I] == '\n')
        {
            printf("\n");
        }
    }
    printf("\nHello world\n");
    FreeBuffer(Buffer);
    return Result;
}

s32 main()
{
    s32 Result = TestSim();
    return Result;
}
