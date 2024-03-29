// TODO: implement carry flag!

/*
  Comments with the DOCS: tag are usually copy-pasted from the 8086 manual:
  https://archive.org/details/bitsavers_intel80869lyUsersManualOct79_62967963
*/


#include "sim.h"
#include "platform.c"

#define MAX_PROGRAM_SIZE 1024

#define HALT_INSTRUCTION 0b11110100

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

#define REGISTER_COUNT 13
u16 GlobalRegisters[REGISTER_COUNT] = {};
#define FLAG_COUNT 9
u16 GlobalFlags = 0;
#define GLOBAL_MEMORY_SIZE 1024 * 1024
u8 GlobalMemory[GLOBAL_MEMORY_SIZE];

s32 RegisterIndexTable[32] = {
    [AX] = 0, [AH] = 0, [AL] = 0,
    [BX] = 1, [BH] = 1, [BL] = 1,
    [CX] = 2, [CH] = 2, [CL] = 2,
    [DX] = 3, [DH] = 3, [DL] = 3,
    [SP] = 4, [BP] = 5, [SI] = 6, [DI] = 7,
    [CS] = 8, [DS] = 9, [SS] = 10, [ES] = 11,
    [IP] = 12,
};

opcode OpcodeTable[OPCODE_COUNT] = {
    [0b100010] = {opcode_kind_RegisterMemoryToFromRegister,instruction_kind_Mov},
    [0b110001] = {opcode_kind_ImmediateToRegisterMemory,instruction_kind_Mov},
    [0b101100] = {opcode_kind_ImmediateToRegister,instruction_kind_Mov},
    [0b101101] = {opcode_kind_ImmediateToRegister,instruction_kind_Mov},
    [0b101110] = {opcode_kind_ImmediateToRegister,instruction_kind_Mov},
    [0b101111] = {opcode_kind_ImmediateToRegister,instruction_kind_Mov},
    [0b101000] = {opcode_kind_MemoryAccumulator,instruction_kind_Mov},
    [0b100011] = {opcode_kind_SegmentRegister,instruction_kind_Mov},
    [0b000000] = {opcode_kind_RegisterMemoryToFromRegister,instruction_kind_Add},
    [0b000001] = {opcode_kind_MemoryAccumulator,instruction_kind_Add},
    [0b001010] = {opcode_kind_RegisterMemoryToFromRegister,instruction_kind_Sub},
    [0b001011] = {opcode_kind_MemoryAccumulator,instruction_kind_Sub},
    [0b001110] = {opcode_kind_RegisterMemoryToFromRegister,instruction_kind_Cmp},
    [0b001111] = {opcode_kind_MemoryAccumulator,instruction_kind_Cmp},
    [0b100000] = {opcode_kind_ImmediateToRegisterMemory,instruction_kind_Derived},
    [0b011101] = {opcode_kind_Jump,instruction_kind_Derived},
    [0b011111] = {opcode_kind_Jump,instruction_kind_Derived},
    [0b011100] = {opcode_kind_Jump,instruction_kind_Derived},
    [0b011110] = {opcode_kind_Jump,instruction_kind_Derived},
    [0b111000] = {opcode_kind_Jump,instruction_kind_Derived},
    [0b111101] = {opcode_kind_Halt,instruction_kind_NONE},
};

// TODO: maybe FullByteOpcodeTable should store opcodes instead of opcode_kinds
opcode_kind FullByteOpcodeTable[256] = {
    [0b11110100] = opcode_kind_Halt,
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

s32 SegmentRegisterTable[4] = {
    [0b00] = ES,
    [0b01] = CS,
    [0b10] = SS,
    [0b11] = DS,
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

#define JUMP_CODE_BITS 8
char *JumpInstructionNameTable[1 << JUMP_CODE_BITS] = {
  [0b01110100] = "je", // jz
  [0b01111100] = "jl", // jnge
  [0b01111110] = "jle", // jng
  [0b01110010] = "jb", // jnae
  [0b01110110] = "jbe", // jna
  [0b01111010] = "jp", // jpe
  [0b01110000] = "jo",
  [0b01111000] = "js",
  [0b01110101] = "jne", // jnz
  [0b01111101] = "jnl", // jge
  [0b01111111] = "jnle", // jg
  [0b01110011] = "jnb", // jae
  [0b01110111] = "jnbe", // ja
  [0b01111011] = "jnp", // jpo
  [0b01110001] = "jno",
  [0b01111001] = "jns",
  [0b11100010] = "loop",
  [0b11100001] = "loopz", // loope
  [0b11100000] = "loopnz", // loopne
  [0b11100011] = "jcxz",
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

static s32 OnesCount(u16 Value)
{
    // NOTE: Hacker's Delight, Figure 5-2
    Value = Value - ((Value >> 1) & 0x55555555);
    Value = (Value & 0x33333333) + ((Value >> 2) & 0x33333333);
    Value = (Value + (Value >> 4)) & 0x0F0F0F0F;
    Value = Value + (Value >> 8);
    Value = Value + (Value >> 16);
    return Value & 0x0000003F;
}

static s32 GetRegisterIndex(s32 RegOrRM, s32 IsWide, s32 IsSegment)
{
    return IsSegment ? SegmentRegisterTable[(RegOrRM & 0b11)] : RegTable[RegOrRM][IsWide];
}

static s16 ReadRegister(register_name RegisterName)
{
    s32 RegisterIndex = RegisterIndexTable[RegisterName];
    switch(RegisterName)
    {
    case AX: case BX: case CX: case DX:
    case SP: case BP: case SI: case DI:
    case CS: case DS: case SS: case ES:
    case IP:
    {
        return 0xffff & GlobalRegisters[RegisterIndex];
    } break;
    case AH: case BH: case CH: case DH:
    {
        return 0xffff & ((GlobalRegisters[RegisterIndex] & 0xff00) >> 8);
    } break;
    case AL: case BL: case CL: case DL:
    {
        return GlobalRegisters[RegisterIndex] & 0xff;
    } break;
    case UNKNOWN_REGISTER: default:
        return ErrorMessageAndCode("Read from unknown register\n", 0x7fffffff);
    }
    return 0;
}

static s32 WriteRegister(register_name RegisterName, s16 Value)
{
    s32 RegisterIndex = RegisterIndexTable[RegisterName];
    switch(RegisterName)
    {
    case AX: case BX: case CX: case DX:
    case SP: case BP: case SI: case DI:
    case CS: case DS: case SS: case ES:
    case IP:
    {
        GlobalRegisters[RegisterIndex] = Value;
    } break;
    case AH: case BH: case CH: case DH:
    {
        GlobalRegisters[RegisterIndex] = ((0xff & Value) << 8) | (0xff & GlobalRegisters[RegisterIndex]);
    } break;
    case AL: case BL: case CL: case DL:
    {
        GlobalRegisters[RegisterIndex] = (0xff & Value) | (0xff00 & GlobalRegisters[RegisterIndex]);
    } break;
    case UNKNOWN_REGISTER: default:
        return ErrorMessageAndCode("Write to unknown register\n", 1);
    }
    return 0;
}

static s16 ReadMemory(s16 MemoryIndex, s32 IsWide)
{
    if (MemoryIndex < 0 || (s32)MemoryIndex >= GLOBAL_MEMORY_SIZE)
    {
        return ErrorMessageAndCode("ReadMemory memory index out-of-bounds\n", 1);
    }
    return GlobalMemory[MemoryIndex];
}

static s32 WriteMemory(s16 MemoryIndex, s16 Value, s32 IsWide)
{
    if (MemoryIndex < 0 || (s32)MemoryIndex >= GLOBAL_MEMORY_SIZE)
    {
        return ErrorMessageAndCode("WriteMemory memory index out-of-bounds\n", 1);
    }
    if (IsWide)
    {
        GlobalMemory[MemoryIndex] = Value;
    }
    else
    {
        u8 HighBits = GlobalMemory[MemoryIndex] & 0xff00;
        GlobalMemory[MemoryIndex] = HighBits | (Value & 0xff);
    }
    return 0;
}

static s16 GetImmediate(s32 Offset, s32 IsWord)
{
    u8 FirstImmediateByte = ReadMemory(ReadRegister(IP) + Offset, 1);
    if (IsWord)
    {
        u8 SecondImmediateByte = ReadMemory(ReadRegister(IP) + Offset + 1, 1);
        return ((0xff & SecondImmediateByte) << 8) | (FirstImmediateByte & 0xff);
    }
    else
    {
        return (FirstImmediateByte << 8) >> 8;
    }
}

static void SetFlag(flag Flag, s32 ShouldSet)
{
    GlobalFlags = ShouldSet ? SET_FLAG(GlobalFlags, Flag) : UNSET_FLAG(GlobalFlags, Flag);
}

static void UpdateFlags(s16 ResultValue)
{
    SetFlag(flag_Zero, ResultValue == 0);
    SetFlag(flag_Sign, (ResultValue & 0x8000) >> 15);
    SetFlag(flag_Parity, OnesCount(ResultValue & 0xff) % 2 == 0);
}

static instruction_kind GetInstructionKindForArithmeticImmediateFromRegisterMemory(s16 REG)
{
    switch(REG)
    {
    case 0b000: return instruction_kind_Add;
    case 0b010: return instruction_kind_Adc;
    case 0b101: return instruction_kind_Sub;
    case 0b011: return instruction_kind_Sbb;
    case 0b111: return instruction_kind_Cmp;
    default: return instruction_kind_NONE;
    }
}

static s32 GetMemoryIndexFromEffectiveAddress(effective_address EffectiveAddress, s32 Offset)
{
    switch(EffectiveAddress)
    {
    case eac_BX: return ReadRegister(BX);
    case eac_BX_SI: return ReadRegister(BX) + ReadRegister(SI);
        // NOTE: eac_BX_D8 falls through to eac_BX_D16, but maybe we need to handle byte-sized offsets differently?
    case eac_BX_D8: case eac_BX_D16:
        return ReadRegister(BX) + Offset;
    case eac_BX_SI_D8: case eac_BX_SI_D16:
        return ReadRegister(BX) + ReadRegister(SI) + Offset;
    case eac_BX_DI_D8: case eac_BX_DI_D16:
        return ReadRegister(BX) + ReadRegister(DI) + Offset;
    case eac_BP_SI_D8: case eac_BP_SI_D16:
        return ReadRegister(BP) + ReadRegister(SI) + Offset;
    case eac_BP_DI_D8: case eac_BP_DI_D16:
        return ReadRegister(BP) + ReadRegister(DI) + Offset;
    case eac_BX_DI: return ReadRegister(BX) + ReadRegister(DI);
    case eac_BP_SI: return ReadRegister(BP) + ReadRegister(SI);
    case eac_BP_DI: return ReadRegister(BP) + ReadRegister(DI);
    case eac_SI: return ReadRegister(SI);
    case eac_DI: return ReadRegister(DI);
    case eac_DIRECT_ADDRESS:
    case eac_SI_D8: case eac_SI_D16:
        return ReadRegister(SI) + Offset;
    case eac_DI_D8: case eac_DI_D16:
        return ReadRegister(DI) + Offset;
    case eac_BP_D8: case eac_BP_D16:
        return ReadRegister(BP) + Offset;
    default:
        printf("EffectiveAddress %s\n", GetEffectiveAddressDisplay(EffectiveAddress));
        return ErrorMessageAndCode("GetMemoryIndexFromEffectiveAddress effective address not implemented\n", -1);
    }
}

static void SetInstructionBufferIndex(s32 Index)
{
    if (Index < 0)
    {
        printf("Invalid insutrction buffer index %d\n", Index);
    }
    // The IP is just the instruction-buffer's index, so we sync the writes here. Maybe at some point it would make more sense to _only_ use the IP register to read the instruction bytes.
    WriteRegister(IP, Index);
}

static s32 InitSimulation(simulation_mode Mode)
{
    switch(Mode)
    {
        case simulation_mode_Print:
            printf("bits 16\n");
    default:
        break;
    }
    return 0;
}

static s32 SimulateRegisterToRegister(simulation_mode Mode, opcode Opcode, s16 DestinationRegister, s16 SourceRegister)
{
    s16 DestinationRegisterValue = ReadRegister(DestinationRegister);
    s16 ValueToWrite = ReadRegister(SourceRegister);
    switch(Mode)
    {
    case simulation_mode_Print:
        printf("%s %s, %s\n", DisplayInstructionKind(Opcode.InstructionKind), DisplayRegisterName(DestinationRegister), DisplayRegisterName(SourceRegister));
        break;
    case simulation_mode_Simulate:
    {
        switch(Opcode.InstructionKind)
        {
        case instruction_kind_Mov:
        {
            WriteRegister(DestinationRegister, ValueToWrite);
        } break;
        case instruction_kind_Add:
        {
            ValueToWrite = DestinationRegisterValue + ValueToWrite;
            UpdateFlags(ValueToWrite);
            WriteRegister(DestinationRegister, ValueToWrite);
        } break;
        case instruction_kind_Adc:
        {
            ValueToWrite = DestinationRegisterValue + ValueToWrite;
            UpdateFlags(ValueToWrite);
            WriteRegister(DestinationRegister, ValueToWrite);
        } break;
        case instruction_kind_Sub:
        {
            ValueToWrite = DestinationRegisterValue - ValueToWrite;
            UpdateFlags(ValueToWrite);
            WriteRegister(DestinationRegister, ValueToWrite);
        } break;
        case instruction_kind_Sbb:
        {
            ValueToWrite = DestinationRegisterValue - ValueToWrite;
            UpdateFlags(ValueToWrite);
            WriteRegister(DestinationRegister, ValueToWrite);
        } break;
        case instruction_kind_Cmp:
        {
            UpdateFlags(DestinationRegisterValue - ValueToWrite);
        } break;
        default: break;
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateRegisterToRegister unknown simulation_mode!\n", 1);
    }
    return 0;
}

static s32 SimulateRegisterAndEffectiveAddress(simulation_mode Mode, opcode Opcode, s16 DestinationRegister, effective_address EffectiveAddress, s16 D, s32 IsDirectAddress, s16 Immediate, s16 Offset)
{
    char DirectAddressDisplay[64];
    char *EffectiveAddressDisplay = GetEffectiveAddressDisplay(EffectiveAddress);
    char *InstructionKindString = DisplayInstructionKind(Opcode.InstructionKind);
    s32 ValueToWrite = 0;
    s32 MemoryIndex = -1;
    s32 IsWide = 1; // TODO: IsWide should be determined in some way, using the effective address or passing in a new IsWide argument.
    if (IsDirectAddress)
    {
        sprintf(DirectAddressDisplay, "[%d]%c", Immediate, 0);
        EffectiveAddressDisplay = DirectAddressDisplay;
    }
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        char *DestinationRegisterString = DisplayRegisterName(DestinationRegister);
        if (D)
        {
            printf("%s %s, %s\n", InstructionKindString, DestinationRegisterString, EffectiveAddressDisplay);
        }
        else
        {
            printf("%s %s, %s\n", InstructionKindString, EffectiveAddressDisplay, DestinationRegisterString);
        }
    } break;
    case simulation_mode_Simulate:
    {
        MemoryIndex = IsDirectAddress ? Immediate : GetMemoryIndexFromEffectiveAddress(EffectiveAddress, Offset);
        s16 MemoryValue = ReadMemory(MemoryIndex + Offset, IsWide);
        s16 RegisterValue = ReadRegister(DestinationRegister);
        switch(Opcode.InstructionKind)
        {
        case instruction_kind_Mov:
            ValueToWrite = D ? MemoryValue : RegisterValue;
            break;
        case instruction_kind_Add:
            ValueToWrite = MemoryValue + RegisterValue;
            UpdateFlags(ValueToWrite);
            break;
        case instruction_kind_Adc:
            ValueToWrite = MemoryValue + RegisterValue;
            UpdateFlags(ValueToWrite);
            break;
        case instruction_kind_Sub:
            ValueToWrite = MemoryValue - RegisterValue;
            UpdateFlags(ValueToWrite);
            break;
        case instruction_kind_Sbb:
            ValueToWrite = MemoryValue - RegisterValue;
            UpdateFlags(ValueToWrite);
            break;
        case instruction_kind_Cmp:
            ValueToWrite = MemoryValue - RegisterValue;
            break;
        default:
            printf("InstructionKind %s\n", InstructionKindString);
            return ErrorMessageAndCode("SimulateRegisterAndEffectiveAddress instruction kind not implemented\n", 1);
        }
        if (D)
        {
            WriteRegister(DestinationRegister, ValueToWrite);
        }
        else
        {
            WriteMemory(MemoryIndex, ValueToWrite, IsWide);
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateRegisterAndEffectiveAddress unknown simulation mode\n", 1);
    }
    return 0;
}

static s32 SimulateImmediateToRegisterMemory(simulation_mode Mode, opcode Opcode, s16 DestinationRegister, s32 IsWide, s16 Immediate, s32 IsMove)
{
    s16 DestinationRegisterValue = ReadRegister(DestinationRegister);
    char *ImmediateSizeName = DisplayByteSize(IsWide);
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        char *RegisterName = DisplayRegisterName(DestinationRegister);
        char *InstructionKindString = DisplayInstructionKind(Opcode.InstructionKind);
        if (IsMove)
        {
            printf("%s %s, %s %d\n", InstructionKindString, RegisterName, ImmediateSizeName, Immediate);
        }
        else
        {
            printf("%s %s %s, %d\n", InstructionKindString, ImmediateSizeName, RegisterName, Immediate);
        }
    } break;
    case simulation_mode_Simulate:
    {
        switch(Opcode.InstructionKind)
        {
        case instruction_kind_Mov:
        {
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Add:
        {
            Immediate = DestinationRegisterValue + Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Adc:
        {
            Immediate = DestinationRegisterValue + Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Sub:
        {
            Immediate = DestinationRegisterValue - Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Sbb:
        {
            Immediate = DestinationRegisterValue - Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Cmp:
        {
            UpdateFlags(DestinationRegisterValue - Immediate);
        } break;
        default:
            return ErrorMessageAndCode("SimulateImmediateToRegisterMemory unkown instruction kind!\n", 1);
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateImmediateToRegisterMemory unknown simulation mode\n", 1);
    }
    return 0;
}

static s32 SimulateImmediateToEffectiveAddressWithOffset(simulation_mode Mode, opcode Opcode, effective_address EffectiveAddress, s32 IsWideDisplacement, s16 Immediate, s16 Displacement, s32 IsMove, s32 IsWide)
{
    char *ImmediateSizeName = DisplayByteSize(IsWide);
    char *EffectiveAddressDisplay = GetEffectiveAddressDisplay(EffectiveAddress);
    s32 MemoryIndex = -1;
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        if (IsMove)
        {
            printf("%s %s %d], %s %d\n", DisplayInstructionKind(Opcode.InstructionKind), EffectiveAddressDisplay, Displacement, ImmediateSizeName, Immediate);
        }
        else
        {
            printf("%s %s %s %d], %d\n", DisplayInstructionKind(Opcode.InstructionKind), ImmediateSizeName, EffectiveAddressDisplay, Displacement, Immediate);
        }
    } break;
    case simulation_mode_Simulate:
    {
        switch(EffectiveAddress)
        {
        case eac_BX_SI_D8: case eac_BX_DI_D8: case eac_BP_SI_D8: case eac_BP_DI_D8:
        case eac_BX_SI_D16: case eac_BX_DI_D16: case eac_BP_SI_D16: case eac_BP_DI_D16:
        case eac_SI_D8: case eac_DI_D8: case eac_BP_D8: case eac_BX_D8:
        case eac_SI_D16: case eac_DI_D16: case eac_BP_D16: case eac_BX_D16:
        {
            MemoryIndex = GetMemoryIndexFromEffectiveAddress(EffectiveAddress, Displacement);
        } break;
        // TODO: nocommit we shouldn't need to check the NON-offset paths, since that is done in SimulateImmediateToEffectiveAddress
        case eac_BX_SI: case eac_BX_DI:
        case eac_BP_SI: case eac_BP_DI:
        case eac_SI: case eac_DI: case eac_BX:
        case eac_DIRECT_ADDRESS: case eac_NONE:
        default:
            printf("EffectiveAddress %d %s\n", EffectiveAddress, GetEffectiveAddressDisplay(EffectiveAddress));
            return ErrorMessageAndCode("SimulateImmediateToEffectiveAddressWithOffset effective address not implememented\n", 1);
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateImmediateToEffectiveAddressWithOffset unknown simulation mode\n", 1);
    }
    WriteMemory(MemoryIndex, Immediate, IsWide);
    return 0;
}

static s32 SimulateImmediateToEffectiveAddress(simulation_mode Mode, opcode Opcode, effective_address EffectiveAddress, s32 IsWide, s16 Immediate, s32 IsMove, s32 IsDirectAddress, s16 DirectAddress)
{
    char *ImmediateSizeName = DisplayByteSize(IsWide);
    char *EffectiveAddressDisplay = GetEffectiveAddressDisplay(EffectiveAddress);
    char DirectAddressDisplay[64];
    if (!IsWide) return ErrorMessageAndCode("SimulateImmediateToEffectiveAddress byte sized instructions not implemented!\n", 1);
    if (IsDirectAddress)
    {
        sprintf(DirectAddressDisplay, "[%d]%c", DirectAddress, 0);
        EffectiveAddressDisplay = DirectAddressDisplay;
    }
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        if (IsMove)
        {
            printf("%s %s, %s %d\n", DisplayInstructionKind(Opcode.InstructionKind), EffectiveAddressDisplay, ImmediateSizeName, Immediate);
        }
        else
        {
            printf("%s %s %s, %d\n", DisplayInstructionKind(Opcode.InstructionKind), ImmediateSizeName, EffectiveAddressDisplay, Immediate);
        }
    } break;
    case simulation_mode_Simulate:
    {
        switch(EffectiveAddress)
        {
        case eac_DIRECT_ADDRESS:
            if (!IsDirectAddress) return ErrorMessageAndCode("SimulateImmediateToEffectiveAddress reached eac_DIRECT_ADDRESS but IsDirectAddress is false!\n", 1);
            return WriteMemory(DirectAddress, Immediate, IsWide);
        case eac_BX_SI: case eac_BX_DI: case eac_BP_SI: case eac_BP_DI:
        case eac_SI: case eac_DI: case eac_BX:
        // TODO: nocommit we shouldn't need to check the offset paths, since that is done in SimulateImmediateToEffectiveAddressWithOffset
        case eac_BX_SI_D8: case eac_BX_DI_D8: case eac_BP_SI_D8: case eac_BP_DI_D8:
        case eac_SI_D8: case eac_DI_D8: case eac_BP_D8: case eac_BX_D8:
        case eac_BX_SI_D16: case eac_BX_DI_D16: case eac_BP_SI_D16: case eac_BP_DI_D16:
        case eac_SI_D16: case eac_DI_D16: case eac_BP_D16: case eac_BX_D16:
        case eac_NONE:
        default:
            printf("EffectiveAddress %d %s\n", EffectiveAddress, GetEffectiveAddressDisplay(EffectiveAddress));
            return ErrorMessageAndCode("SimulateImmediateToEffectiveAddress effective address not implememented\n", 1);
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateImmediateToEffectiveAddress not implemented!\n", 1);
    }
    return 0;
}

static s32 SimulateImmediateToRegister(simulation_mode Mode, opcode Opcode, s16 DestinationRegister, s16 Immediate)
{
    s32 DestinationRegisterIndex = RegisterIndexTable[DestinationRegister];
    s16 DestinationRegisterValue = GlobalRegisters[DestinationRegisterIndex];
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        printf("%s %s, %d\n", DisplayInstructionKind(Opcode.InstructionKind), DisplayRegisterName(DestinationRegister), Immediate);
    } break;
    case simulation_mode_Simulate:
    {
        switch(Opcode.InstructionKind)
        {
        case instruction_kind_Mov:
        {
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Add:
        {
            Immediate = DestinationRegisterValue + Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Adc:
        {
            Immediate = DestinationRegisterValue + Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Sub:
        {
            Immediate = DestinationRegisterValue - Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Sbb:
        {
            Immediate = DestinationRegisterValue - Immediate;
            UpdateFlags(Immediate);
            WriteRegister(DestinationRegister, Immediate);
        } break;
        case instruction_kind_Cmp:
        {
            UpdateFlags(DestinationRegisterValue - Immediate);
        } break;
        default: break;
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateImmediateToRegister unknown simulation mode\n", 1);
    }
    return 0;
}

static s32 SimulateMemoryAccumulator(simulation_mode Mode, opcode Opcode, s16 Immediate, s16 D, s32 IsMove, s32 IsWideData)
{
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        char *AccumulatorRegister = IsWideData ? "ax" : "al";
        if (D)
        {
            char *Format = IsMove ? "%s [%d], %s\n" : "%s %d, %s\n";
            printf(Format, DisplayInstructionKind(Opcode.InstructionKind), Immediate, AccumulatorRegister);
        }
        else
        {
            char *Format = IsMove ? "%s %s, [%d]\n" : "%s %s, %d\n";
            printf(Format, DisplayInstructionKind(Opcode.InstructionKind), AccumulatorRegister, Immediate);
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateMemoryAccumulator not implemented!\n", 1);
    }
    return 0;
}

static s32 SimulateJump(simulation_mode Mode, s8 InstructionOffset)
{
    s16 InstructionPointer = ReadRegister(IP);
    s16 InstructionValue = ReadMemory(InstructionPointer, 1);
    char *JumpInstructionName = JumpInstructionNameTable[InstructionValue];
    s32 JumpIndex = InstructionPointer + InstructionOffset;
    switch(Mode)
    {
    case simulation_mode_Print:
    {
        printf("%s $+2+%d\n", JumpInstructionName, InstructionOffset);
    } break;
    case simulation_mode_Simulate:
    {
        switch(InstructionValue)
        {
        case JE:
        {
            if (GET_FLAG(GlobalFlags, flag_Zero)) SetInstructionBufferIndex(JumpIndex);
        } break;
        case JNE:
        {
            if (!GET_FLAG(GlobalFlags, flag_Zero)) SetInstructionBufferIndex(JumpIndex);
        } break;
        case JL: case JNL:
        case JLE: case JNLE:
        case JB: case JNB:
        case JBE: case JNBE:
        case JP: case JNP:
        case JO: case JNO:
        case JS: case JNS:
        case LOOP:
        case LOOPZ:
        case LOOPNZ:
        case JCXZ:
        default:
            printf("Jump Instruction %s\n", JumpInstructionName);
            return ErrorMessageAndCode("SimulateJump instruction not implemented\n", 1);
        }
    } break;
    default:
        return ErrorMessageAndCode("SimulateJump unknown simulation mode\n", 1);
    }
    return 0;
}

static void DEBUG_PrintGlobalRegisters()
{
    char *NameMap[] = {"AX", "BX", "CX", "DX", "SP", "BP", "SI", "DI", "CS", "DS", "SS", "ES", "IP"};
    s32 I;
    printf("----------------------\nRegisters:\n");
    for (I = 0; I < REGISTER_COUNT; ++I)
    {
        if (GlobalRegisters[I]) printf("  %s %0004x \n", NameMap[I], GlobalRegisters[I]);
    }
    printf("\nFlags:           DIOSZAPC\n      ");
    DEBUG_PrintByteInBinary(0xff & (GlobalFlags >> 8));
    DEBUG_PrintByteInBinary(0xff & GlobalFlags);
    printf("\n");
}

static s32 SimulateInstructions(simulation_mode Mode)
{
    s32 Result = 0, Running = 1;
    if (InitSimulation(Mode)) return ErrorMessageAndCode("Error initializing simulation\n", 1);
    while(Running && Result == 0)
    {
        if (1 && Mode != simulation_mode_Print) DEBUG_PrintGlobalRegisters();
        u8 FirstByte = ReadMemory(ReadRegister(IP), 1);
        u8 OpcodeValue = GET_OPCODE(FirstByte);
        opcode Opcode = OpcodeTable[OpcodeValue];
        opcode_kind FullByteOpcodeKind = FullByteOpcodeTable[FirstByte];
        if (FullByteOpcodeKind)
        {
            // NOTE: hack because the simulator started off only parsing 6-bit opcodes.....
            Opcode = (opcode){FullByteOpcodeKind,0};
        }
        s16 D = GET_D(FirstByte);
        s16 W = GET_W(FirstByte);
        s32 InstructionLength = 2; /* we just guess that InstructionLength is 2 and update it in places where it is not */
        switch(Opcode.Kind)
        {
        case opcode_kind_SegmentRegister:
        case opcode_kind_RegisterMemoryToFromRegister:
        {
            s32 IsSegment = Opcode.Kind == opcode_kind_SegmentRegister;
            u8 SecondByte = ReadMemory(ReadRegister(IP) + 1, 1);
            s16 MOD = GET_MOD(SecondByte);
            s16 REG = GET_REG(SecondByte);
            s16 RM = GET_RM(SecondByte);
            if(MOD == 0b11)
            {
                s16 RegIndex = IsSegment ? SegmentRegisterTable[(REG & 0b11)] : RegTable[REG][W];
                s32 RmIsWide = IsSegment || W;
                s16 RmIndex = RegTable[RM][RmIsWide];
                s16 DestinationRegister = D ? RegIndex : RmIndex;
                s16 SourceRegister      = D ? RmIndex  : RegIndex;
                Result = SimulateRegisterToRegister(Mode, Opcode, DestinationRegister, SourceRegister);
            }
            else
            {
                s16 DestinationRegister = GetRegisterIndex(REG, W, IsSegment);
                effective_address EffectiveAddress = EffectiveAddressCalculationTable[MOD][RM];
                if (MOD == 0b01 || MOD == 0b10)
                {
                    InstructionLength = MOD == 0b10 ? 4 : 3;
                    s16 Immediate = GetImmediate(2, MOD == 0b10);
                    Result = SimulateRegisterAndEffectiveAddress(Mode, Opcode, DestinationRegister, EffectiveAddress, D, 0, 0, Immediate);
                }
                else
                {
                    // NOTE: MOD == 0b00
                    s32 IsDirectAddress = EffectiveAddress == eac_DIRECT_ADDRESS;
                    s16 Immediate = 0;
                    if (IsDirectAddress)
                    {
                        Immediate = GetImmediate(2, 1);
                        InstructionLength = 4;
                    }
                    Result = SimulateRegisterAndEffectiveAddress(Mode, Opcode, DestinationRegister, EffectiveAddress, D, IsDirectAddress, Immediate, 0);
                }
            }
        } break;
        case opcode_kind_ImmediateToRegisterMemory:
        {
            u8 SecondByte = ReadMemory(ReadRegister(IP) + 1, 1);
            s32 IsMove = OpcodeValue == MOV_IMMEDIATE_TO_REGISTER_MEMORY;
            s32 IsMoveAndWideData = IsMove && W;
            s32 IsWideData = IsMoveAndWideData || (!IsMove && !D && W);
            s16 MOD = GET_MOD(SecondByte);
            s16 REG = GET_REG(SecondByte);
            s16 RM = GET_RM(SecondByte);
            if (OpcodeValue == 0b100000)
            {
                Opcode.InstructionKind = GetInstructionKindForArithmeticImmediateFromRegisterMemory(REG);
            }
            if(MOD == 0b11)
            {
                InstructionLength = IsWideData ? 4 : 3;
                s16 Immediate = GetImmediate(2, IsWideData);
                s16 DestinationRegister = RegTable[RM][W];
                Result = SimulateImmediateToRegisterMemory(Mode, Opcode, DestinationRegister, W, Immediate, IsMove);
            }
            else
            {
                effective_address EffectiveAddress = EffectiveAddressCalculationTable[MOD][RM];
                if (MOD == 0b01 || MOD == 0b10)
                {
                    s16 IsWideDisplacement = MOD == 0b10;
                    if (MOD == 0b01)
                    {
                        InstructionLength = IsWideData ? 5 : 4;
                    }
                    else
                    {
                        InstructionLength = IsWideData ? 6 : 5;
                    }
                    s16 Displacement = GetImmediate(2, IsWideDisplacement);
                    s16 ImmediateOffset = MOD == 0b10 ? 4 : 3;
                    s16 Immediate = GetImmediate(ImmediateOffset, IsWideData);
                    printf("Immediate %d       Displacement %d\n", Immediate, Displacement);
                    Result = SimulateImmediateToEffectiveAddressWithOffset(Mode, Opcode, EffectiveAddress, IsWideDisplacement, Immediate, Displacement, IsMove, W);
                }
                else
                {
                    // MOD == 0b00
                    // TODO: add in check for direct address
                    s16 Immediate = GetImmediate(2, IsWideData);
                    s16 DirectAddress = 0;
                    s32 IsDirectAddress = EffectiveAddress == eac_DIRECT_ADDRESS;
                    InstructionLength = IsWideData ? 4 : 3;
                    if (IsDirectAddress)
                    {
                        DirectAddress = GetImmediate(2, 1);
                        Immediate = GetImmediate(4, IsWideData);
                        InstructionLength = IsWideData ? 6 : 5;
                    }
                    Result = SimulateImmediateToEffectiveAddress(Mode, Opcode, EffectiveAddress, W, Immediate, IsMove, IsDirectAddress, DirectAddress);
                }
            }
        } break;
        case opcode_kind_ImmediateToRegister:
        {
            s16 REG = GET_IMMEDIATE_TO_REGISTER_REG(FirstByte);
            s16 W = GET_IMMEDIATE_TO_REGISTER_W((s32)FirstByte);
            s16 DestinationRegister = RegTable[REG][W];
            s16 Immediate = GetImmediate(1, W);
            InstructionLength = W ? 3 : 2;
            Result = SimulateImmediateToRegister(Mode, Opcode, DestinationRegister, Immediate);
        } break;
        case opcode_kind_MemoryAccumulator:
        {
            s32 IsMove = OpcodeValue == MOV_ACCUMULATOR_TO_FROM_MEMORY;
            s32 IsWideData = IsMove || W;
            InstructionLength = IsWideData ? 3 : 2;
            s16 Immediate = GetImmediate(1, IsWideData);
            Result = SimulateMemoryAccumulator(Mode, Opcode, Immediate, D, IsMove, IsWideData);
        } break;
        case opcode_kind_RegisterToRegisterMemory:
            return ErrorMessageAndCode("opcode_kind_RegisterToRegisterMemory not implemented\n", -1);
        case opcode_kind_Jump:
        {
            s8 InstructionOffset = GetImmediate(1, 0);
            InstructionLength = 2;
            Result = SimulateJump(Mode, InstructionOffset);
        } break;
        case opcode_kind_Halt:
            // NOTE: set InstructionLength just to make it easier to check with the reference simulator
            InstructionLength = 0;
            Running = 0;
            break;
        default:
            printf("FirstByte"); DEBUG_PrintByteInBinary(FirstByte); printf("\n");
            return ErrorMessageAndCode("SimulateInstructions default error\n", -1);
        }
        SetInstructionBufferIndex(ReadRegister(IP) + InstructionLength);
    }
    if (!Result && Mode != simulation_mode_Print) DEBUG_PrintGlobalRegisters();
    return Result;
}

static s32 TestSim(simulation_command_line_args CommandLineArgs)
{
    s32 I, SimResult = 0;
    simulation_mode Mode = simulation_mode_Simulate;
    char *FilePaths[] = {
        /* "../assets/listing_0039_more_movs", */
        /* "../assets/listing_0040_challenge_movs", */
        /* "../assets/listing_0041_add_sub_cmp_jnz", */
        /* "../assets/listing_0043_immediate_movs", */
        /* "../assets/listing_0044_register_movs", */
        /* "../assets/listing_0045_challenge_register_movs", */
        /* "../assets/listing_0046_add_sub_cmp", */
        /* "../assets/listing_0047_challenge_flags", */
        /* "../assets/listing_0048_ip_register", */
        /* "../assets/listing_0049_conditional_jumps", */
        /* "../assets/listing_0051_memory_mov", */
        /* "../assets/listing_0052_memory_add_loop", */
        /* "../assets/listing_0053_add_loop_challenge", */
        "../assets/listing_0054_draw_rectangle",
    };

    for (I = 0; I < ARRAY_COUNT(FilePaths); ++I)
    {
        // Zero out simulation memory between simulations
        memset(GlobalMemory, 0, GLOBAL_MEMORY_SIZE);
        buffer *Buffer = ReadFileIntoBuffer(FilePaths[I]);
        memcpy(GlobalMemory, Buffer->Data, Buffer->Size);
        // Put a HALT instruction at the end of the program
        GlobalMemory[Buffer->Size] = HALT_INSTRUCTION;
        if(!Buffer)
        {
            printf("Error reading file %s\n", FilePaths[I]);
            continue;
        }

        printf("; %s\n", FilePaths[I]);
        FreeBuffer(Buffer);
        SimResult = SimulateInstructions(Mode);
        if (CommandLineArgs.DumpMemory)
        {
            FILE *file = fopen("../dist/memory_dump.data", "wb");
            fwrite(GlobalMemory, 1, GLOBAL_MEMORY_SIZE, file);
            fclose(file);
        }
    }
    return SimResult;
}

static s32 StringMatch(char *StringA, char *StringB)
{
    s32 I = 0, Result = 1;
    while (StringA[I] != 0 && StringB[I] != 0)
    {
        if (StringA[I] != StringB[I])
        {
            Result = 0;
            break;
        }
        ++I;
    }
    return Result;
}

static simulation_command_line_args ParseArgs(int ArgCount, char **Args)
{
    s32 I;
    simulation_command_line_args CommandLineArgs = {0};
    if (ArgCount > 1)
    {
        for (I = 1; I < ArgCount; ++I)
        {
            if (StringMatch(Args[I], "-d") || StringMatch(Args[I], "--dump"))
            {
                CommandLineArgs.DumpMemory = 1;
            }
        }
    }
    return CommandLineArgs;
}

int main(int ArgCount, char **Args)
{
    simulation_command_line_args CommandLineArgs = ParseArgs(ArgCount, Args);
    int Result = TestSim(CommandLineArgs);
    return Result;
}
