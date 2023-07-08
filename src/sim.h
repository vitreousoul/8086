#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;

typedef int8_t s8;
typedef int32_t s32;
typedef int16_t s16;

typedef size_t size;

#define ARRAY_COUNT(a) ((s32)(sizeof(a) / sizeof((a)[0])))

typedef enum
{
   opcode_kind_None,
   opcode_kind_RegisterMemoryToFromRegister,
   opcode_kind_ImmediateToRegisterMemory,
   opcode_kind_ImmediateToRegister,
   opcode_kind_MemoryAccumulator,
   opcode_kind_SegmentRegister,
   opcode_kind_RegisterToRegisterMemory,
   opcode_kind_Jump,
} opcode_kind;

typedef enum
{
    instruction_kind_NONE,
    instruction_kind_Derived,
    instruction_kind_Mov,
    instruction_kind_Add,
    instruction_kind_Adc,
    instruction_kind_Sub,
    instruction_kind_Sbb,
    instruction_kind_Cmp,
} instruction_kind;

typedef struct
{
    opcode_kind Kind;
    instruction_kind InstructionKind;
} opcode;

typedef enum
{
    UNKNOWN_REGISTER,
    AX, AH, AL,
    BX, BH, BL,
    CX, CH, CL,
    DX, DH, DL,
    SP, BP, SI, DI,
    CS, DS, SS, ES,
} register_name;

typedef enum
{
    eac_NONE,
    eac_BX_SI,
    eac_BX_DI,
    eac_BP_SI,
    eac_BP_DI,
    eac_SI,
    eac_DI,
    eac_DIRECT_ADDRESS,
    eac_BX,
    eac_BX_SI_D8,
    eac_BX_DI_D8,
    eac_BP_SI_D8,
    eac_BP_DI_D8,
    eac_SI_D8,
    eac_DI_D8,
    eac_BP_D8,
    eac_BX_D8,
    eac_BX_SI_D16,
    eac_BX_DI_D16,
    eac_BP_SI_D16,
    eac_BP_DI_D16,
    eac_SI_D16,
    eac_DI_D16,
    eac_BP_D16,
    eac_BX_D16,
} effective_address;

typedef enum
{
    simulation_mode_Print,
    simulation_mode_Simulate,
} simulation_mode;

static char *DisplayOpcodeKind(opcode_kind Kind)
{
    switch(Kind)
    {
    case opcode_kind_RegisterMemoryToFromRegister: return "opcode_kind_RegisterMemoryToFromRegister";
    case opcode_kind_ImmediateToRegisterMemory: return "opcode_kind_ImmediateToRegisterMemory";
    case opcode_kind_ImmediateToRegister: return "opcode_kind_ImmediateToRegister";
    case opcode_kind_MemoryAccumulator: return "opcode_kind_MemoryAccumulator";
    case opcode_kind_SegmentRegister: return "opcode_kind_SegmentRegister";
    case opcode_kind_RegisterToRegisterMemory: return "opcode_kind_RegisterToRegisterMemory";
    case opcode_kind_None: default: return "opcode_kind_None";
    }
}

static char *DisplayRegisterName(register_name Name)
{
    switch(Name)
    {
    case AH: return "AH";
    case AL: return "AL";
    case AX: return "AX";
    case BH: return "BH";
    case BL: return "BL";
    case BP: return "BP";
    case BX: return "BX";
    case CH: return "CH";
    case CL: return "CL";
    case CX: return "CX";
    case DH: return "DH";
    case DI: return "DI";
    case DL: return "DL";
    case DX: return "DX";
    case SI: return "SI";
    case SP: return "SP";
    case CS: return "CS";
    case DS: return "DS";
    case SS: return "SS";
    case ES: return "ES";
    default:
    case UNKNOWN_REGISTER: return "UNKNOWN_REGISTER";
    }
}

static char *DisplayInstructionKind(instruction_kind Kind)
{
    switch(Kind)
    {
    case instruction_kind_Mov: return "mov";
    case instruction_kind_Add: return "add";
    case instruction_kind_Adc: return "adc";
    case instruction_kind_Sub: return "sub";
    case instruction_kind_Sbb: return "sbb";
    case instruction_kind_Cmp: return "cmp";

    default: return "UNKNOWN INSTRUCTION KIND";
    }
}

static void DEBUG_PrintByteInBinary(u8 Byte)
{
    s32 I;
    printf(" ");
    for (I = 7; I >= 0; --I)
    {
        printf("%d", (Byte >> I) & 0b1);
    };
    printf(" ");
}
