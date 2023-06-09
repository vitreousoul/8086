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
#define OPCODE_BITS 6
#define OPCODE_MASK 0b111111
#define GET_OPCODE(b) (OPCODE_MASK & ((b) >> 2))

/*
  DOCS: The following bit, called the D field, generally specifies the "direction" of the operation:
  1 = the REG field in the second byte identifies the destination operand,
  0 = the REG field identifies the source operand
*/
#define D_BITS 1
#define D_MASK 0b1
#define GET_D(b) (D_MASK & ((b) >> 1))

/*
  DOCS: The W field distinguishes between byte and word operations: 0 = byte, 1 = word.
*/
#define W_BITS 1
#define W_MASK 0b1
#define GET_W(b) (W_MASK & (b))
#define GET_IMMEDIATE_TO_REGISTER_W(b) (W_MASK & ((b) >> 3))

/*
  DOCS: The second byte of the instruction usually identifies the instruction's operands.
  The MOD (mode) field indicates whether one of the operands is in memory or whether both operands are registers.
*/
#define MOD_BITS 2
#define MOD_MASK 0b11
#define GET_MOD(b) (MOD_MASK & ((b) >> 6))

/*
  DOCS: The REG (register) field identifies a register that is one of the instruction operands.
*/
#define REG_BITS 3
#define REG_MASK 0b111
#define GET_REG(b) (REG_MASK & ((b) >> 3))
#define GET_IMMEDIATE_TO_REGISTER_REG(b) (REG_MASK & (b))

/*
  DOCS: The encoding of the R/M (register/memory) field depends on how the mode field is set.
  If MOD = 11 (register-to-register mode), then R/M identifies the second register operand.
  If MOD selects memory mode, then R/M indicates how the effective address of the memory operand is to be calculated.
*/
#define RM_BITS 3
#define RM_MASK 0b111
#define GET_RM(b) (RM_MASK & (b))

#define OPCODE_COUNT (1 << OPCODE_BITS)
#define MOD_COUNT (1 << MOD_BITS)
#define REG_COUNT (1 << REG_BITS)
#define W_COUNT (1 << REG_BITS)
#define RM_COUNT (1 << RM_BITS)

opcode OpcodeTable[OPCODE_COUNT] = {
    [0b100010] = {opcode_kind_RegisterMemoryToFromRegister},
    [0b110001] = {opcode_kind_ImmediateToRegisterMemory},
    [0b101100] = {opcode_kind_ImmediateToRegister},
    [0b101101] = {opcode_kind_ImmediateToRegister},
    [0b101110] = {opcode_kind_ImmediateToRegister},
    [0b101111] = {opcode_kind_ImmediateToRegister},
    [0b101000] = {opcode_kind_MemoryAccumulator},
    [0b100011] = {opcode_kind_SegmentRegister},
};

s32 ModTable[MOD_COUNT] = {
    [0b00] = -1, /* DOCS: Memory Mode, no displacement follows except when R/M = 110, then 16-bit displacement follows */
    [0b01] = -1, /* DOCS: Memory Mode, 8-bit displacement follows */
    [0b10] = -1, /* DOCS: Memory Mode, 16-bit displacement follows */
    [0b11] = -1, /* DOCS: Register Mode (no displacement) */
};

s32 RegTable[REG_COUNT][W_COUNT] = {
    [0b000] = {AL,AX},
    [0b001] = {CL,CX},
    [0b010] = {DL,DX},
    [0b011] = {BL,BX},
    [0b100] = {AH,SP},
    [0b101] = {CH,BP},
    [0b110] = {DH,SI},
    [0b111] = {BH,DI},
};

s32 EffectiveAddressCalculationTable[MOD_COUNT][RM_COUNT] = {
    [0b00] = {
        eac_BX_SI,
        eac_BX_DI,
        eac_BP_SI,
        eac_BP_DI,
        eac_SI,
        eac_DI,
        eac_DIRECT_ADDRESS,
        eac_BX,
    },
    [0b01] = {
        eac_BX_SI_D8,
        eac_BX_DI_D8,
        eac_BP_SI_D8,
        eac_BP_DI_D8,
        eac_SI_D8,
        eac_DI_D8,
        eac_BP_D8,
        eac_BX_D8,
    },
    [0b10] = {
        eac_BX_SI_D16,
        eac_BX_DI_D16,
        eac_BP_SI_D16,
        eac_BP_DI_D16,
        eac_SI_D16,
        eac_DI_D16,
        eac_BP_D16,
        eac_BX_D16,
    },
};

static s32 SimulateBuffer(buffer *OpcodeBuffer)
{
    /* TODO: define simulated values as s16 since it should run in 16-bit mode eventually */
    s32 Result = 0;
    printf("bits 16\n");
    while(1)
    {
        if(OpcodeBuffer->Index >= OpcodeBuffer->Size)
        {
            /* TODO: there should be an error here sometimes. Like if there is a lone byte at the end of the instruction stream */
            break;
        }
        u8 FirstByte = OpcodeBuffer->Data[OpcodeBuffer->Index];
        u8 OpcodeValue = GET_OPCODE(FirstByte);
        opcode Opcode = OpcodeTable[OpcodeValue];
        s16 D = GET_D(FirstByte);
        s16 W = GET_W(FirstByte);
        s32 InstructionLength = 2; /* we just guess that InstructionLength is 2 and update it in places where it is not */
        switch(Opcode.Kind)
        {
        case opcode_kind_RegisterMemoryToFromRegister:
        {
            u8 SecondByte = OpcodeBuffer->Data[OpcodeBuffer->Index + 1];
            s16 MOD = GET_MOD(SecondByte);
            s16 REG = GET_REG(SecondByte);
            s16 RM = GET_RM(SecondByte);
            if(MOD == 0b11)
            {
                s16 DestinationIndex = D ? REG : RM;
                s16 SourceIndex      = D ? RM  : REG;
                s16 DestinationRegister = RegTable[DestinationIndex][W];
                s16 SourceRegister      = RegTable[SourceIndex     ][W];
                printf("mov %s, %s\n", DisplayRegisterName(DestinationRegister), DisplayRegisterName(SourceRegister));
            }
            else
            {
                printf("Unimplemented: %s\n", DisplayOpcodeKind(Opcode.Kind));
                return -1;
            }
        } break;
        case opcode_kind_ImmediateToRegisterMemory:
            printf("opcode_kind_ImmediateToRegisterMemory\n");
            return -1;
        case opcode_kind_ImmediateToRegister:
        {
            u8 SecondByte = OpcodeBuffer->Data[OpcodeBuffer->Index + 1];
            /* u8 SignBit = (0b1 & (SecondByte >> 7)); */
            s16 REG = GET_IMMEDIATE_TO_REGISTER_REG(FirstByte);
            s16 W = GET_IMMEDIATE_TO_REGISTER_W((s32)FirstByte);
            s16 DestinationRegister = RegTable[REG][W];
            s16 Immediate = (SecondByte << 24) >> 24; /* this feels dirty.... but it's a way to retain sign-extension */
            if (W)
            {
                if (OpcodeBuffer->Index + 2 >= OpcodeBuffer->Size)
                {
                    printf("opcode_kind_ImmediateToRegister unexpected end of buffer\n");
                    return -1;
                }
                u8 ThirdByte = OpcodeBuffer->Data[OpcodeBuffer->Index + 2];
                /* TODO: handle wide W=1 immediate, which needs to look at a third byte */
                Immediate = ((0b11111111 & ThirdByte) << 8) | (SecondByte & 0b11111111);
                InstructionLength = 3;
            }
            printf("mov %s, %d\n", DisplayRegisterName(DestinationRegister), Immediate);
        } break;
        case opcode_kind_MemoryAccumulator:
            printf("opcode_kind_MemoryAccumulator\n");
            return -1;
        case opcode_kind_SegmentRegister:
            printf("opcode_kind_SegmentRegister\n");
            return -1;
        case opcode_kind_RegisterToRegisterMemory:
            printf("opcode_kind_RegisterToRegisterMemory\n");
            return -1;
        default:
        {
            printf("Error\n");
            return -1;
        }
        }
        OpcodeBuffer->Index += InstructionLength;
    }
    printf("\n");
    return Result;
}

static s32 TestSim(void)
{
    s32 SimResult = 0;
    char *FilePath = "../assets/listing_0039_more_movs";
    buffer *Buffer = ReadFileIntoBuffer(FilePath);
    if(!Buffer)
    {
        printf("Error reading file %s\n", FilePath);
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
