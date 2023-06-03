#include "sim.h"
#include "platform.c"

#define OPCODE_MASK 0b111111
#define GET_OPCODE_FROM_BYTE(b) (((b) >> 2) & OPCODE_MASK)

#define OPCODE_TAIL_MASK 0b11
#define GET_OPCODE_TAIL(b) ((b) & OPCODE_MASK)

#define OPCODE_BIT_WIDTH 6
#define OPCODE_TABLE_COUNT (1 << OPCODE_BIT_WIDTH)
opcode OpcodeTable[OPCODE_TABLE_COUNT] = {
    [0b100010] = {opcode_kind_RegisterMemoryToFromRegister,2},
    [0b110001] = {opcode_kind_ImmediateToRegisterMemory,2},
    [0b101100] = {opcode_kind_ImmediateToRegister,2},
    [0b101101] = {opcode_kind_ImmediateToRegister,2},
    [0b101110] = {opcode_kind_ImmediateToRegister,2},
    [0b101111] = {opcode_kind_ImmediateToRegister,2},
    [0b101000] = {opcode_kind_MemoryAccumulator,2},
    [0b100011] = {opcode_kind_SegmentRegister,2},
};

static s32 SimulateBuffer(buffer *OpcodeBuffer)
{
    s32 I = 0, Success = 1;
    while(1)
    {
        if(I >= OpcodeBuffer->Size)
        {
            break;
        }
        printf("byte %x\n", OpcodeBuffer->Data[I]);
        u8 OpcodeValue = GET_OPCODE_FROM_BYTE(OpcodeBuffer->Data[I]);
        opcode Opcode = OpcodeTable[OpcodeValue];
        printf("opcode kind %s\n", DisplayOpcodeKind(Opcode.Kind));
        switch(Opcode.Kind)
        {
        case opcode_kind_RegisterMemoryToFromRegister:
        {
            printf("mov XXX, XXX\n");
            /* printf("0x%x %s\n", Buffer->Data[I], DisplayOpcodeKind(OpcodeTable[Opcode])); */
        } break;
        case opcode_kind_ImmediateToRegisterMemory:
        case opcode_kind_ImmediateToRegister:
        case opcode_kind_MemoryAccumulator:
        case opcode_kind_SegmentRegister:
        case opcode_kind_RegisterToRegisterMemory:
        default:
        {
            printf("Error\n");
            return -1;
        }
        }
        I += Opcode.Length;
    }
    printf("\n");
    return Success;
}

static s32 TestSim(void)
{
    s32 SimResult = 0;
    buffer *Buffer = ReadFileIntoBuffer("../assets/test");
    if(!Buffer)
    {
        return 1;
    }
    SimResult = SimulateBuffer(Buffer);
    FreeBuffer(Buffer);
    return SimResult;
}

int main(void)
{
    int Result = TestSim();
    return Result;
}
