#include "sim.h"
#include "platform.c"

/*
  NOTE: for now we will emit assembly instructions from the instruction buffer, to test if we
  properly interpreted the byte-stream
*/
static s32 SimulateInstructionBuffer(buffer *InstructionBuffer)
{
    s32 I, Success = 1;
    printf("buffer\n");
    for(I = 0; I < InstructionBuffer->Size; ++I)
    {
        printf("0x%x ", InstructionBuffer->Data[I]);
        if(InstructionBuffer->Data[I] == '\n')
        {
            printf("\n");
        }
    }
    printf("\nHello world\n");
    return Success;
}

static s32 TestSim()
{
    s32 SimResult = 0;
    buffer *Buffer = ReadFileIntoBuffer("../assets/test.txt");
    if(!Buffer)
    {
        return 1;
    }
    SimResult = SimulateInstructionBuffer(Buffer);
    FreeBuffer(Buffer);
    return SimResult;
}

s32 main()
{
    s32 Result = TestSim();
    return Result;
}
