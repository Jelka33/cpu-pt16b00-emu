#include <stdio.h>
#include <stdint.h>

#include "arch.h"

void writeBEuint16(uint16_t* arr, int n, FILE* fptr) {
    for (int i = 0; i < n; i++) {
        unsigned char num[2];
        num[0] = arr[i] >> 8;
        num[1] = arr[i];
        fwrite(&num, 2, 1, fptr);
    }
}

int main() {
    /*
    Code 1:
    ; arithmetic and logic
    add %r0, 1
    add %r1, 2
    add %r0, %r1    ; %r0 = 3

    add %r0, 5      ; %r0 = 8
    add %r1, %r0    ; %r1 = 10

    add %r2, 3
    sub %r1, %r2    ; %r1 = 7
    sub %r1, 4      ; %r1 = 3
    sub %r0, 6      ; %r0 = 2

    add %r3, 10
    mul %r2, 2      ; %r2 = 6
    mul %r3, %r2    ; %r3 = 60

    mul %r0, 0
    mul %r1, 0
    add %r0, b11110000
    add %r1, b10110011
    and %r0, %r1
    and %r1, b01010101

    or %r0, %r1
    or %r1, b10110011

    xor %r0, %r1
    xor %r1, b11111111
    xor %r1, %r1
    xor %r0, %r0

    add %r0, b11010011
    shr %r0
    shr %r0
    shr %r0
    sar %r0
    or %r0, b10000000_00000000
    sar %r0
    sar %r0
    shr %r0
    shr %r0

    hlt
    ; memory
    load %r0, 0
    load %r1, 2

    xor %r0, %r0
    xor %r1, %r1
    add %r0, x1324
    add %r1, x5768
    xor %r5, %r5
    xor %r4, %r4
    add %r5, xdead
    add %r4, xf00d
    write1b 0
    load %r0, 0
    write2b 1
    load %r1, 1
    write3b 2
    load %r0, 2
    load %r1, 3
    write4b 4
    load %r0, 4
    load %r1, 5

    hlt
    mov %r0, %r4
    mov %r1, 9876
    mov %spt, stack     ; substitute "stack" with the index
                        ; of the last element of the payload
    push %r1
    push 0xdaba
    pop %r0
    pushf
    popf

    hlt
    ; branching
    xor %r0, %r0
    sjmp 3
    mov %r0, 0xdead     ; should not be executed
    sjmp 4
    jmp next_mov        ; substitute "next_mov" with the address
                        ; of the next mov instruction
    sjmp -3
    mov %r0, 0xc01d
    ; TODO: add aditional branching tests after adding flags support

    hlt
    ; stack (10 words)
    nop ; 10 times
    */

    uint16_t code1[] = {
        // arithmetic and logic
        0b10, 1, 0b001000010, 2, 0b001000000001,

        0b10, 5, 0b000001000001,

        0b010000010, 3, 0b010001000101, 0b001000110, 4, 0b110, 6,

        0b011000010, 10, 0b010001010, 2, 0b010011001001,

        0b001010, 0, 0b001001010, 0, 0b10, 0b11110000, 0b001000010, 0b10110011,
        0b001000001011, 0b001001100, 0b01010101,

        0b001000001101, 0b001001110, 0b10110011,

        0b001000001111, 0b001010000, 0b11111111, 0b001001001111, 0b000000001111,

        0b10, 0b11010011, 0b010001, 0b010001, 0b010001, 0b010010,
        0b001110, 0b1000000000000000, 0b010010, 0b010010, 0b010001, 0b010001,

        0b110011,
        // memory
        0b010101, 0, 0b001010101, 2, 0b001111, 0b001001001111, 0b10, 0x1324, 0b001000010, 0x5768,
        0b101101001111, 0b100100001111, 0b101000010, 0xdead, 0b100000010, 0xf00d,
        0b010110, 0, 0b010101, 0, 0b010111, 1, 0b001010101, 1, 0b011000, 2, 0b010101, 2, 0b001010101, 3,
        0b011001, 4, 0b010101, 4, 0b001010101, 5,

        0b110011,
        0b100000011010, 0b001011011, 9876, 0b110011011, 122, // <- put here the index of the last element!!!
        0b001011100, 0b011101, 0xdaba, 0b011110, 0b011111, 0b100000,

        0b110011,
        // branching
        0b1111, 0b011101000, 0b11011, 0xdead, 0b100101000, 0b100001, 110,0, // <- 32-bit index of the next mov operation
        0b1111111101101000, 0b11011, 0xc01d,

        0b110011,
        // stack (10 words)
        0,0,0,0,0, 0,0,0,0,0,
    };

    FILE* fptr = fopen("./bin/tests/test1.bin", "wb");
    writeBEuint16(code1, sizeof(code1)/sizeof(uint16_t), fptr);
    fclose(fptr);

    /*
    Code 2:
    ; calls, interrupts, ivt
    mov %spt, stack ; put address of the last element in stack

    mov %r0, 1
    mov %r1, 3
    call swap       ; put address of "swap"
    
    setivt ivt      ; put address of "ivt"
    int 0
    int 1

    hlt
    sjmp -1

    ; functions and ISRs
    swap:
        mov %r2, %r0
        mov %r0, %r1
        mov %r1, %r2
        ret
    
    write2mem:
        mov %r5, %r1
        mul %r0, 256    ; shift 8 bits left
        or %r5, %r0

        write2b 0

        ; 8 bits right shift
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0

        iret

    readmem:
        load %r0, 0
        mov %r1, %r0

        ; 8 bits right shift
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0
        shr %r0

        and %r1, 0b11111111

        iret

    ; stack (10 words)
    nop ; 10 times

    ; ivt
    write2mem   ; put address of "write2mem"
    readmem     ; put address of "readmem"
    */

    uint16_t code2[] = {
        0b110011011, 58 /*stack address*/, 0b011011, 1, 0b001011011, 3, 0b101111, 16,0 /*address of "swap"*/,
        0b110110, 59,0 /*ivt address*/, 0b110001, 0b1110001,

        0b110011, 0b1111111111101000,

        // swap:
        0b000010011010, 0b001000011010, 0b010001011010, 0b110000,

        // write2mem:
        0b001101011010, 0b001010, 256, 0b000101001101, 0b010111, 0,
        0b010001, 0b010001, 0b010001, 0b010001, 0b010001, 0b010001, 0b010001, 0b010001, // right shift
        0b110010,

        // readmem:
        0b010101, 0, 0b000001011010,
        0b010001, 0b010001, 0b010001, 0b010001, 0b010001, 0b010001, 0b010001, 0b010001, // right shift
        0b001001100, 255, 0b110010,

        // stack (10 words)
        0,0,0,0,0, 0,0,0,0,0,

        // ivt
        20,0 /*address of "write2mem"*/, 35,0 /*address of "readmem"*/,
    };

    fptr = fopen("./bin/tests/test2.bin", "wb");
    writeBEuint16(code2, sizeof(code1)/sizeof(uint16_t), fptr);
    fclose(fptr);

    /*
    Code 3:
    mov %r0, 0
    mov %r5, 0xf00d

    write2b %r0
    add %r0, 1
    write2b %r0
    add %r0, 1
    write2b %r0
    add %r0, 1
    write2b %r0
    add %r0, 1
    write2b %r0
    add %r0, 1

    hlt
    nop
    */

    uint16_t code3[] = {
        0b011011, 0, 0b101011011, 0xf00d,
        0b111001, 0b10, 1,
        0b111001, 0b10, 1,
        0b111001, 0b10, 1,
        0b111001, 0b10, 1,
        0b111001, 0b10, 1,
        0b110011,
        0
    };

    fptr = fopen("./bin/tests/test3.bin", "wb");
    writeBEuint16(code3, sizeof(code1)/sizeof(uint16_t), fptr);
    fclose(fptr);

    /*
    Code 4:
    mov %r0, buffer
    mov %r5, 0

    ; loop
    write2b %r0
    add %r0, 1
    add %r5, 1
    cmp %r0, buffer+9
    sjb -7
    hlt

    mov %r0, 0
    jz stop
    mov %r0, 1

    buffer:
    nop ; 10 times

    stop:
    hlt
    sjmp -1

    hlt
    nop
    */

    uint16_t code4[] = {
        0b011011, 20 /*address of "buffer"*/, 0b101011011, 0,

        // loop
        0b111001, 0b10, 1, 0b101000010, 1, 0b010100, 20 /*address of "buffer"*/ +9,
        0b1111111001101110, 0b110011,

        0b011011, 0, 0b100010, 30,0 /*address of "stop"*/, 0b011011, 1,

        // buffer
        0,0,0,0,0, 0,0,0,0,0,

        // stop
        0b110011, 0b1111111111101000,

        0b110011,
        0
    };

    fptr = fopen("./bin/tests/test4.bin", "wb");
    writeBEuint16(code4, sizeof(code1)/sizeof(uint16_t), fptr);
    fclose(fptr);

    return 0;
}