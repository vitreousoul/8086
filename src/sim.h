#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;

typedef int32_t s32;

typedef size_t size;

typedef enum
{
   instruction_kind_RegisterMemoryToFromRegister,
   instruction_kind_ImmediateToRegisterMemory,
   instruction_kind_ImmediateToRegister,
   instruction_kind_MemoryAccumulator,
   instruction_kind_SegmentRegister,
   instruction_kind_RegisterToRegisterMemory,
   /* TODO: fill out the rest of the instructions */
} instruction_kind;
