#!/usr/bin/env sh

nasm assets/listing_0041_add_sub_cmp_jnz.asm
./test.sh > assets/out_test.asm
nasm assets/out_test.asm
diff assets/out_test assets/listing_0041_add_sub_cmp_jnz
