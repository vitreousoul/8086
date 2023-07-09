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

#define SET_FLAG(flags, flag) ((flags) | (flag))
#define UNSET_FLAG(flags, flag) ((flags) & (~flag))

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
    // Status Flags
    ///////////////
    // DOCS: If CF (flag_Carry) is set, there has been a carry out of, or a borrow into, the high-order but of the result (8- or 16-bit quantity). This flag is used by decimal arithmetic instructions.
    flag_Carry = 0x1,
    // DOCS: If PF (flag_Parity) is set, the result has even parity, an even number of 1-buts. This flag can be used to check for data transmission errors.
    flag_Parity = 0x2,
    // DOCS: If AF (flag_Aux_Carry) is set, there has been a carry out of the low-nibble into the high-nibble or a borrow from the high-nibble into the low-nibble of an 8-bit quantity (low-order byte of a 16-bit quantity). This flag is used by decimal arithmetic instructions.
    flag_Aux_Carry = 0x4,
    // DOCS: If ZF (flag_Zero) is set, the result of the operation is 0.
    flag_Zero = 0x8,
    // DOCS: If SF (flag_Sign) is set, the high-order bit of the result is a 1. Since negative binary numbers are represented in the 8086 and 8088 in standard two's complement notation, SF indicates the sign of the result (0 = positive, 1 = negative).
    flag_Sign = 0x10,
    // DOCS: If OF (flag_Overflow) is set, an arithmetic overflow has occured; that is, a significant digit has been lost because the size of the result exceeded capacity of its destination location. An Interrupt On Overflow instruction is availble that will generate an interrupt in this situation.
    flag_Overflow = 0x20,
    // Control Flags
    ////////////////
    // DOCS: Setting IF (flag_Interrupt) allows the CPU to recognize external (maskable) interrupt requests. Clearing IF disables these interrupts. IF has no affect on either non-maskable external or internally generated interrupts.
    flag_Interrupt = 0x40,
    // DOCS: Setting DF (flag_Direction) causes string instructions to auto-decrement; that is, to process strings from high addresses to low addresses, or from "right to left." Clearing DF causes string instructions to auto-increment, or to process string from "left to right."
    flag_Direction = 0x80,
    // DOCS: Settings TF (flag_Trap) puts the processor into single-step mode for debugging. In this mode, CPU automatically generates an internal interrupt after each instruction, allowing a program to be inspected as it executes instruction-by-instruction.
    flag_Trap = 0x100,
} flag;

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
