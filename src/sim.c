#include "sim.h"
#include "platform.c"

#define OPCODE_MASK 0b111111
#define GET_OPCODE(b) (((b) >> 2) & OPCODE_MASK)

#define OPCODE_TAIL_MASK 0b11
/* TODO: should GET_OPCODE_TAIL shift down? isn't the tail at the end, and so we just mask without any shifts? */
#define GET_OPCODE_TAIL(b) ((b) & OPCODE_MASK)

#define OPCODE_BIT_WIDTH 6
#define OPCODE_TABLE_COUNT (1 << OPCODE_BIT_WIDTH)
s32 OpcodeTable[OPCODE_TABLE_COUNT] = {
    [0b100010] = instruction_kind_RegisterMemoryToFromRegister,
    [0b110001] = instruction_kind_ImmediateToRegisterMemory,
    [0b101100] = instruction_kind_ImmediateToRegister,
    [0b101101] = instruction_kind_ImmediateToRegister,
    [0b101110] = instruction_kind_ImmediateToRegister,
    [0b101111] = instruction_kind_ImmediateToRegister,
    [0b101000] = instruction_kind_MemoryAccumulator,
    [0b100011] = instruction_kind_SegmentRegister,
};

static s32 InstructionSize(u8 Byte)
{
    s32 Opcode = GET_OPCODE(Byte);
    s32 OpcodeTail = GET_OPCODE_TAIL(Byte);

}

static void DecodeInstructionBuffer(buffer *InstructionBuffer)
{
    s32 I = 0;
    while(I < InstructionBuffer->Size)
    {
    }
}

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
