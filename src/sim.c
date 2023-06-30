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

#define MOV_IMMEDIATE_TO_REGISTER_MEMORY 0b110001
#define MOV_ACCUMULATOR_TO_FROM_MEMORY 0b101000

opcode OpcodeTable[OPCODE_COUNT] = {
    [0b100010] = {opcode_kind_RegisterMemoryToFromRegister,"mov"},
    [0b110001] = {opcode_kind_ImmediateToRegisterMemory,"mov"},
    [0b101100] = {opcode_kind_ImmediateToRegister,"mov"},
    [0b101101] = {opcode_kind_ImmediateToRegister,"mov"},
    [0b101110] = {opcode_kind_ImmediateToRegister,"mov"},
    [0b101111] = {opcode_kind_ImmediateToRegister,"mov"},
    [0b101000] = {opcode_kind_MemoryAccumulator,"mov"},
    [0b100011] = {opcode_kind_SegmentRegister,"mov"},
    [0b000000] = {opcode_kind_RegisterMemoryToFromRegister,"add"},
    [0b000001] = {opcode_kind_MemoryAccumulator,"add"},
    [0b001010] = {opcode_kind_RegisterMemoryToFromRegister,"sub"},
    [0b001011] = {opcode_kind_MemoryAccumulator,"sub"},
    [0b001110] = {opcode_kind_RegisterMemoryToFromRegister,"cmp"},
    [0b001111] = {opcode_kind_MemoryAccumulator,"cmp"},
    [0b100000] = {opcode_kind_ImmediateToRegisterMemory,"OPCODE_NAME_DERIVED_FROM_REG"},
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

static char *GetEffectiveAddressDisplay(effective_address EffectiveAddress)
{
    switch(EffectiveAddress)
    {
    case eac_NONE: return "eac_NONE";
    case eac_BX_SI: return "[bx + si]";
    case eac_BX_DI: return "[bx + di]";
    case eac_BP_SI: return "[bp + si]";
    case eac_BP_DI: return "[bp + di]";
    case eac_SI: return "[si]";
    case eac_DI: return "[di]";
    case eac_DIRECT_ADDRESS: return "eac_DIRECT_ADDRESS";
    case eac_BX: return "[bx]";
    case eac_BX_SI_D8: return "[bx + si +";
    case eac_BX_DI_D8: return "[bx + di +";
    case eac_BP_SI_D8: return "[bp + si +";
    case eac_BP_DI_D8: return "[bp + di +";
    case eac_SI_D8: return "[si +";
    case eac_DI_D8: return "[di +";
    case eac_BP_D8: return "[bp +";
    case eac_BX_D8: return "[bx +";
    case eac_BX_SI_D16: return "[bx + si +";
    case eac_BX_DI_D16: return "[bx + di +";
    case eac_BP_SI_D16: return "[bp + si +";
    case eac_BP_DI_D16: return "[bp + di +";
    case eac_SI_D16: return "[si +";
    case eac_DI_D16: return "[di +";
    case eac_BP_D16: return "[bp +";
    case eac_BX_D16: return "[bx +";
    }
}

static s32 ErrorMessageAndCode(char *Message, s32 Code)
{
    printf("ERROR: %s", Message);
    return Code;
}

static s16 GetImmediate(buffer *OpcodeBuffer, s32 Offset, s32 IsWord)
{
    s32 FirstImmediateByte = OpcodeBuffer->Data[OpcodeBuffer->Index + Offset];
    s16 Immediate = (FirstImmediateByte << 24) >> 24;
    if (IsWord)
    {
        u8 SecondImmediateByte = OpcodeBuffer->Data[OpcodeBuffer->Index + Offset + 1];
        Immediate = ((0b11111111 & SecondImmediateByte) << 8) | (Immediate & 0b11111111);
    }
    return Immediate;
}

static char *GetOpcodeNameForArithmeticImmediateFromRegisterMemory(s16 REG)
{
    switch(REG)
    {
    case 0b000: return "add";
    case 0b010: return "adc";
    case 0b101: return "sub";
    case 0b011: return "sbb";
    case 0b111: return "cmp";
    default: return "INVALID";
    }
}

static s32 SimulateBuffer(buffer *OpcodeBuffer)
{
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
        char *OpcodeName = Opcode.Name;
        s16 D = GET_D(FirstByte);
        s16 W = GET_W(FirstByte);
        s32 InstructionLength = 2; /* we just guess that InstructionLength is 2 and update it in places where it is not */
        char DirectAddressDisplay[64];
        switch(Opcode.Kind)
        {
        case opcode_kind_RegisterMemoryToFromRegister:
        {
            if (OpcodeBuffer->Index + 1 >= OpcodeBuffer->Size)
            {
                return ErrorMessageAndCode("opcode_kind_RegisterMemoryToFromRegister unexpected end of buffer\n", 1);
            }
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
                printf("%s %s, %s\n", OpcodeName, DisplayRegisterName(DestinationRegister), DisplayRegisterName(SourceRegister));
            }
            else
            {
                s16 DestinationRegister = RegTable[REG][W];
                effective_address EffectiveAddress = EffectiveAddressCalculationTable[MOD][RM];
                char *EffectiveAddressDisplay = GetEffectiveAddressDisplay(EffectiveAddress);
                if (MOD == 0b01 || MOD == 0b10)
                {
                    s32 LastByteOffset = MOD == 0b10 ? 3 : 2;
                    if (OpcodeBuffer->Index + LastByteOffset >= OpcodeBuffer->Size)
                    {
                        return ErrorMessageAndCode("opcode_kind_RegisterMemoryToFromRegister unexpected end of buffer\n", 1);
                    }
                    InstructionLength = MOD == 0b10 ? 4 : 3;
                    s16 Immediate = GetImmediate(OpcodeBuffer, 2, MOD == 0b10);
                    if (D)
                    {
                        printf("%s %s, %s %d]\n", OpcodeName, DisplayRegisterName(DestinationRegister), EffectiveAddressDisplay, Immediate);
                    }
                    else
                    {
                        printf("%s %s %d], %s\n", OpcodeName, EffectiveAddressDisplay, Immediate, DisplayRegisterName(DestinationRegister));
                    }
                }
                else
                {
                    // NOTE: MOD == 0b00
                    if (EffectiveAddress == eac_DIRECT_ADDRESS)
                    {
                        if (OpcodeBuffer->Index + 3 >= OpcodeBuffer->Size)
                        {
                            return ErrorMessageAndCode("opcode_kind_RegisterMemoryToFromRegister unexpected end of buffer\n", 1);
                        }
                        s16 Immediate = GetImmediate(OpcodeBuffer, 2, 1);
                        sprintf(DirectAddressDisplay, "[%d]%c", Immediate, 0);
                        EffectiveAddressDisplay = DirectAddressDisplay;
                        InstructionLength = 4;
                    }
                    if (D)
                    {
                        printf("%s %s, %s\n", OpcodeName, DisplayRegisterName(DestinationRegister), EffectiveAddressDisplay);
                    }
                    else
                    {
                        printf("%s %s, %s\n", OpcodeName, EffectiveAddressDisplay, DisplayRegisterName(DestinationRegister));
                    }
                }
            }
        } break;
        case opcode_kind_ImmediateToRegisterMemory:
        {
            if (OpcodeBuffer->Index + 1 > OpcodeBuffer->Size)
            {
                return ErrorMessageAndCode("Unexpected end-of-stream while parsing opcode_kind_ImmediateToRegisterMemory", 1);
            }
            u8 SecondByte = OpcodeBuffer->Data[OpcodeBuffer->Index + 1];
            s32 IsMove = OpcodeValue == MOV_IMMEDIATE_TO_REGISTER_MEMORY;
            s32 IsMoveAndWideData = IsMove && W;
            s32 IsWideData = IsMoveAndWideData || (!IsMove && !D && W);
            char *ImmediateSizeName = W ? "word" : "byte";
            s16 MOD = GET_MOD(SecondByte);
            s16 REG = GET_REG(SecondByte);
            s16 RM = GET_RM(SecondByte);
            if (OpcodeValue == 0b100000)
            {
                OpcodeName = GetOpcodeNameForArithmeticImmediateFromRegisterMemory(REG);
            }
            if(MOD == 0b11)
            {
                s32 NeededByteCount = IsWideData ? 3 : 2;
                if (OpcodeBuffer->Index + NeededByteCount > OpcodeBuffer->Size)
                {
                    return ErrorMessageAndCode("Unexpected end-of-stream while parsing opcode_kind_ImmediateToRegisterMemory", 1);
                }
                InstructionLength = IsWideData ? 4 : 3;
                s16 Immediate = GetImmediate(OpcodeBuffer, 2, IsWideData);
                s16 DestinationRegister = RegTable[RM][W];
                if (IsMove)
                {
                    printf("%s %s, %s %d\n", OpcodeName, DisplayRegisterName(DestinationRegister), ImmediateSizeName, Immediate);
                }
                else
                {
                    printf("%s %s %s, %d\n", OpcodeName, ImmediateSizeName, DisplayRegisterName(DestinationRegister), Immediate);
                }
            }
            else
            {
                effective_address EffectiveAddress = EffectiveAddressCalculationTable[MOD][RM];
                char *EffectiveAddressDisplay = GetEffectiveAddressDisplay(EffectiveAddress);
                if (MOD == 0b01 || MOD == 0b10)
                {
                    s16 IsWideDisplacement = MOD == 0b10;
                    ImmediateSizeName = IsWideDisplacement ? "word" : "byte";
                    if (MOD == 0b01)
                    {
                        InstructionLength = IsWideData ? 5 : 4;
                    }
                    else
                    {
                        InstructionLength = IsWideData ? 6 : 5;
                    }
                    s16 Displacement = GetImmediate(OpcodeBuffer, 2, IsWideDisplacement);
                    s16 ImmediateOffset = MOD == 0b10 ? 4 : 3;
                    if (OpcodeBuffer->Index + ImmediateOffset + IsWideData >= OpcodeBuffer->Size)
                    {
                        return ErrorMessageAndCode("opcode_kind_ImmediateToRegisterMemory unexpected end of buffer\n", 1);
                    }
                    s16 Immediate = GetImmediate(OpcodeBuffer, ImmediateOffset, IsWideData);
                    if (IsMove)
                    {
                        printf("%s %s %d], %s %d\n", OpcodeName, EffectiveAddressDisplay, Displacement, ImmediateSizeName, Immediate);
                    }
                    else
                    {
                        printf("%s %s %s %d], %d\n", OpcodeName, ImmediateSizeName, EffectiveAddressDisplay, Displacement, Immediate);
                    }
                }
                else
                {
                    // MOD == 0b00
                    // TODO: add in check for direct address
                    s16 Immediate = GetImmediate(OpcodeBuffer, 2, IsWideData);
                    InstructionLength = IsWideData ? 4 : 3;
                    if (EffectiveAddress == eac_DIRECT_ADDRESS)
                    {
                        s16 DirectAddress = GetImmediate(OpcodeBuffer, 2, 1);
                        Immediate = GetImmediate(OpcodeBuffer, 4, IsWideData);
                        sprintf(DirectAddressDisplay, "[%d]%c", DirectAddress, 0);
                        EffectiveAddressDisplay = DirectAddressDisplay;
                        InstructionLength = IsWideData ? 6 : 5;
                    }
                    if (IsMove)
                    {
                        printf("%s %s, %s %d\n", OpcodeName, EffectiveAddressDisplay, ImmediateSizeName, Immediate);
                    }
                    else
                    {
                        printf("%s %s %s, %d\n", OpcodeName, ImmediateSizeName, EffectiveAddressDisplay, Immediate);
                    }
                }
            }
        } break;
        case opcode_kind_ImmediateToRegister:
        {
            s16 REG = GET_IMMEDIATE_TO_REGISTER_REG(FirstByte);
            s16 W = GET_IMMEDIATE_TO_REGISTER_W((s32)FirstByte);
            s16 DestinationRegister = RegTable[REG][W];
            s32 OverflowCheckOffset = W ? 2 : 1;
            if (OpcodeBuffer->Index + OverflowCheckOffset >= OpcodeBuffer->Size)
            {
                return ErrorMessageAndCode("opcode_kind_ImmediateToRegister unexpected end of buffer\n", -1);
            }
            s16 Immediate = GetImmediate(OpcodeBuffer, 1, W);
            InstructionLength = W ? 3 : 2;
            printf("%s %s, %d\n", OpcodeName, DisplayRegisterName(DestinationRegister), Immediate);
        } break;
        case opcode_kind_MemoryAccumulator:
        {
            if (OpcodeBuffer->Index + 1 >= OpcodeBuffer->Size)
            {
                return ErrorMessageAndCode("opcode_kind_MemoryAccumulator unexpected end of buffer\n", 1);
            }
            s32 IsMove = OpcodeValue == MOV_ACCUMULATOR_TO_FROM_MEMORY;
            s32 IsWideData = IsMove || W;
            char *AccumulatorRegister = IsWideData ? "ax" : "al";
            InstructionLength = IsWideData ? 3 : 2;
            s16 Immediate = GetImmediate(OpcodeBuffer, 1, IsWideData);
            if (D)
            {
                char *Format = IsMove ? "%s [%d], %s\n" : "%s %d, %s\n";
                printf(Format, OpcodeName, Immediate, AccumulatorRegister);
            }
            else
            {
                char *Format = IsMove ? "%s %s, [%d]\n" : "%s %s, %d\n";
                printf(Format, OpcodeName, AccumulatorRegister, Immediate);
            }
        } break;
        case opcode_kind_SegmentRegister:
            return ErrorMessageAndCode("opcode_kind_SegmentRegister not implemented\n", -1);
        case opcode_kind_RegisterToRegisterMemory:
            return ErrorMessageAndCode("opcode_kind_RegisterToRegisterMemory not implemented\n", -1);
        default:
            printf("FirstByte"); DEBUG_PrintByteInBinary(FirstByte); printf("\n");
            return ErrorMessageAndCode("SimulateBuffer default error\n", -1);
        }
        OpcodeBuffer->Index += InstructionLength;
    }
    printf("\n");
    return Result;
}

static s32 TestSim(void)
{
    s32 I, SimResult = 0;
    char *FilePaths[] = {
        /* "../assets/listing_0039_more_movs", */
        /* "../assets/listing_0040_challenge_movs", */
        "../assets/listing_0041_add_sub_cmp_jnz"
        /* "../assets/test" */
    };

    for (I = 0; I < ARRAY_COUNT(FilePaths); ++I)
    {
        buffer *Buffer = ReadFileIntoBuffer(FilePaths[I]);
        if(!Buffer)
        {
            printf("Error reading file %s\n", FilePaths[I]);
            continue;
        }
        printf("; %s", FilePaths[I]);
        SimResult = SimulateBuffer(Buffer);
        FreeBuffer(Buffer);
    }
    return SimResult;
}

int main(void)
{
    int Result = TestSim();
    return Result;
}
