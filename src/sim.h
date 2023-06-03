#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint8_t u8;

typedef int32_t s32;

typedef size_t size;

typedef enum
{
   instruction_kind_None,
   instruction_kind_RegisterMemoryToFromRegister,
   instruction_kind_ImmediateToRegisterMemory,
   instruction_kind_ImmediateToRegister,
   instruction_kind_MemoryAccumulator,
   instruction_kind_SegmentRegister,
   instruction_kind_RegisterToRegisterMemory,
   /* TODO: fill out the rest of the instructions */
} instruction_kind;

static char *DisplayInstructionKind(instruction_kind Kind)
{
    switch(Kind)
    {
    case instruction_kind_None: return "instruction_kind_None";
    case instruction_kind_RegisterMemoryToFromRegister: return "instruction_kind_RegisterMemoryToFromRegister";
    case instruction_kind_ImmediateToRegisterMemory: return "instruction_kind_ImmediateToRegisterMemory";
    case instruction_kind_ImmediateToRegister: return "instruction_kind_ImmediateToRegister";
    case instruction_kind_MemoryAccumulator: return "instruction_kind_MemoryAccumulator";
    case instruction_kind_SegmentRegister: return "instruction_kind_SegmentRegister";
    case instruction_kind_RegisterToRegisterMemory: return "instruction_kind_RegisterToRegisterMemory";
    }
}
