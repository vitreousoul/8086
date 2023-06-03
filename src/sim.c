#include "sim.h"
#include "platform.c"

#define OPCODE_MASK 0b111111
#define GET_OPCODE(b) (((b) >> 2) & OPCODE_MASK)

#define OPCODE_TAIL_MASK 0b11
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

static s32 GetInstructionSize(buffer *InstructionBuffer, s32 Index)
{
    return 2;
}

/*
  NOTE: for now we will emit assembly instructions from the instruction buffer, to test if we
  properly interpreted the byte-stream
*/
static s32 SimulateInstructionBuffer(buffer *InstructionBuffer)
{
    s32 I, Success = 1;
    for(I = 0; I < InstructionBuffer->Size; ++I)
    {
        u8 Opcode = GET_OPCODE(InstructionBuffer->Data[I]);
        instruction_kind InstructionKind = OpcodeTable[Opcode];
        s32 InstructionSize = GetInstructionSize(InstructionBuffer, I);
        switch(InstructionKind)
        {
        case instruction_kind_RegisterMemoryToFromRegister:
        {
            printf("mov XXX, XXX\n");
            /* printf("0x%x %s\n", InstructionBuffer->Data[I], DisplayInstructionKind(OpcodeTable[Opcode])); */
        } break;
        case instruction_kind_ImmediateToRegisterMemory:
        case instruction_kind_ImmediateToRegister:
        case instruction_kind_MemoryAccumulator:
        case instruction_kind_SegmentRegister:
        case instruction_kind_RegisterToRegisterMemory:
        default:
        {
            printf("Error\n");
            return -1;
        }
        }
    }
    printf("\n");
    return Success;
}

static s32 TestSim()
{
    s32 SimResult = 0;
    buffer *Buffer = ReadFileIntoBuffer("../assets/test1");
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
