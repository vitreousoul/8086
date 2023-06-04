/*
  Comments with the DOCS: tag are usually copy-pasted from the 8086 manual:
  https://archive.org/details/bitsavers_intel80869lyUsersManualOct79_62967963
*/

#include "sim.h"
#include "platform.c"

/*
  DOCS: The first six bits of a multibyte instruction generally contain an opcode
  that identifies the basic instruction type: ADD, XOR, etc.
*/
#define OPCODE_MASK 0b111111
#define GET_OPCODE(b) (OPCODE_MASK & ((b) >> 2))
/*
  DOCS: The following bit, called the D field, generally specifies the "direction" of the operation:
  1 = the REG field in the second byte identifies the destination operand,
  0 = the REG field identifies the source operand
*/
#define D_MASK 0b1
#define GET_D(b) (D_MASK & ((b) >> 1))
/*
  DOCS: The W field distinguishes between byte and word operations: 0 = byte, 1 = word.
*/
#define W_MASK 0b1
#define GET_W(b) (W_MASK & (b))
/*
  DOCS: The second byte of the instruction usually identifies the instruction's operands.
  The MOD (mode) field indicates whether one of the operands is in memory or whether both operands are registers.
*/
#define MOD_MASK 0b11
#define GET_MOD(b) (MOD_MASK & ((b) >> 6))
/*
  DOCS: The REG (register) field identifies a register that is one of the instruction operands.
*/
#define REG_MASK 0b111
#define GET_REG(b) (REG_MASK & ((b) >> 3))
/*
  DOCS: The encoding of the R/M (register/memory) field depends on how the mode field is set.
  If MOD = 11 (register-to-register mode), then R/M identifies the second register operand.
  If MOD selects memory mode, then R/M indicates how the effective address of the memory operand is to be calculated.
*/
#define RM_MASK 0b111
#define GET_RM(b) (RM_MASK & (b))


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
    s32 Result = 0;
    while(1)
    {
        if(OpcodeBuffer->Index >= OpcodeBuffer->Size - 1)
        {
            /* check (Size - 1) because we assume the opcode is at least two bytes long */
            /*
              TODO: there should be an error here sometimes.
              Like if there is a lone byte at the end of the instruction stream
            */
            break;
        }
        u8 OpcodeValue = GET_OPCODE(OpcodeBuffer->Data[OpcodeBuffer->Index]);
        opcode Opcode = OpcodeTable[OpcodeValue];
        printf("opcode kind %s\n", DisplayOpcodeKind(Opcode.Kind));
        switch(Opcode.Kind)
        {
        case opcode_kind_RegisterMemoryToFromRegister:
        {
            printf("mov XXX, XXX\n");
            /* printf("0x%x %s\n", Buffer->Data[OpcodeBuffer->Index], DisplayOpcodeKind(OpcodeTable[Opcode])); */
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
        OpcodeBuffer->Index += Opcode.Length;
    }
    printf("\n");
    return Result;
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
