#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;

typedef int32_t s32;

typedef size_t size;

typedef enum
{
   opcode_kind_None,
   opcode_kind_RegisterMemoryToFromRegister,
   opcode_kind_ImmediateToRegisterMemory,
   opcode_kind_ImmediateToRegister,
   opcode_kind_MemoryAccumulator,
   opcode_kind_SegmentRegister,
   opcode_kind_RegisterToRegisterMemory
} opcode_kind;

typedef struct
{
    opcode_kind Kind;
    s32 Length;
} opcode;

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
