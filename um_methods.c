/*
 * um_methods.c
 * Dan Meyer and Quinn Collins
 * 11/24/2015
 * implementation of all 15 UM assembler instructions. These instructions are
 * stored in a method suite that is put into an array. Also defines struct
 * Three_regs which holds the values of 3 registers, to simplify getting and
 * storing registers for 3-register functions.  
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "um_methods.h"
#include "bitpack.h"
#include "memory_manager.h"

#define MAXVAL 4294967296
#define REGSIZE 4
#define NUMFUNCS 15

/* method suite which is stored in array for easy use of UM assembler
 * instructions in memory_manager and um.c
 */
cmd_ptr instr_array[NUMFUNCS] = {CMOV, SLOAD, SSTORE, ADD, MULT, DIV,
                                 NAND, HALT, MAP, UNMAP, OUTPUT, INPUT,
                                 LOADP, LOADV};

typedef struct Three_regs {
        unsigned *a;
        unsigned *b;
        unsigned *c;
} Three_regs;

/* Gets pointers to three registers and stores them in a Three_regs struct. 
 * Used in each three-register function. 
 */
static inline Three_regs get_three_regs(Mem memory, unsigned cw)
{
        Three_regs tr = {
                get_register(memory, Bitpack_getu(cw, 3, 6)),
                get_register(memory, Bitpack_getu(cw, 3, 3)),
                get_register(memory, Bitpack_getu(cw, 3, 0))
        };
        return tr;
}

/* conditional move: if $r[c] != 0, $r[a] := $r[b] */
void CMOV(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        if (*tr.c != 0) {
                *tr.a = *tr.b;
        }
}

/* segmented load: $r[a] := $m[$r[b]$r[c]] */
void SLOAD(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        Seg cur_seg;
        cur_seg = (Seg)Seq_get(memory->main_mem, *tr.b);
        *tr.a = *((unsigned*)UArray_at(cur_seg, *tr.c));
}

/* segmented store: $m[$r[a]$r[b]] := $r[c] */
void SSTORE(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        Seg cur_seg;
        cur_seg = (Seg)Seq_get(memory->main_mem, *tr.a);
        *((unsigned*)UArray_at(cur_seg, *tr.b)) = *tr.c;
}

/* addition: $r[a] := ($r[b] + $r[c]) % 2^32 */
void ADD(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        *tr.a = (*tr.b + *tr.c) % MAXVAL;
}

/* multiplication: $r[a] := ($r[b] * $r[c]) % 2^32 */
void MULT(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        *tr.a = ((*tr.b) * (*tr.c) ) % MAXVAL;
}

/* division: $r[a] := floor($r[b] / $r[c]) */
void DIV(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        *tr.a = *tr.b / *tr.c;
}

/* bitwise NAND: $r[a] := !($r[b] & $r[c]) */
void NAND(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        *tr.a = ~((*tr.b) & (*tr.c));
}

/* halt: computation stops */
void HALT(Mem memory, unsigned cw)
{
        (void)cw;
        free_memory(memory);
        exit(0);
}

/* map segment: $r[b] is given the segment identifier. New segment is
 * initialized with $r[c] words */
void MAP(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        unsigned seg_index = 0;
        Seg new_seg = UArray_new(*tr.c, REGSIZE);
        if (Stack_empty(memory->free_regs) != 1) {
                seg_index = (unsigned)(uintptr_t)Stack_pop(memory->free_regs);
                Seq_put(memory->main_mem, seg_index, (void*)new_seg);
        }
        else {
                Seq_addhi(memory->main_mem, new_seg);
                seg_index = Seq_length(memory->main_mem) - 1;
        }
        *tr.b = seg_index;
}

/* unmap segment: segment at $r[c] is unmapped. Future map instructions may
 * reuse the identifier */
void UNMAP(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        Seg cur_seg;
        cur_seg = Seq_put(memory->main_mem, *tr.c, NULL);
        Stack_push(memory->free_regs, 
                   (void*)(uintptr_t)(*tr.c));
        UArray_free(&cur_seg);
}

/* output: $r[c] is displayed to I/O. Only values 0-255 permitted */
void OUTPUT(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        int output = *tr.c;
        assert(output < 256);
        putchar(output);
}

/* input: UM waits for input from I/O and puts it in $r[c]. only values 0-255
 * permitted. if end of input, fill $r[c] with 1s */
void INPUT(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);
        int input  = getchar();
        *tr.c = input;
}

/* load program: $m[$r[b]] is duplicated and moved to $m[0]. program counter is
 * set to $m[0][$r[c]]. */
void LOADP(Mem memory, unsigned cw)
{
        Three_regs tr = get_three_regs(memory, cw);

        Seg new_seg;
        new_seg = (Seg)Seq_get(memory->main_mem, *tr.b);
        Seg old_seg = (Seg)Seq_get(memory->main_mem, 0);
        if (*tr.b != 0)
                UArray_free(&old_seg);
        Seq_put(memory->main_mem, 0, new_seg);
        memory->pcount = *tr.c; }

/* load value: loads value into specified register */
void LOADV(Mem memory, unsigned cw)
{
        unsigned val = Bitpack_getu(cw, 25, 0);
        int reg = Bitpack_getu(cw, 3, 25);
        *get_register(memory, reg) = val;
}