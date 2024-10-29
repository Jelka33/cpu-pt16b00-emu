#ifndef EMULATOR_ARCH_H_
#define EMULATOR_ARCH_H_

#include <stdint.h>

extern uint16_t reg_file[8];
extern uint32_t pc;
extern uint16_t spth;
extern uint32_t ivtaddr;

extern uint8_t ALU_FLAGS;
#define ZERO_FLAG       (ALU_FLAGS & 0b0001)
#define SIGN_FLAG       ((ALU_FLAGS & 0b0010) >> 1)
#define OVERFLOW_FLAG   ((ALU_FLAGS & 0b0100) >> 2)
#define CARRY_FLAG      ((ALU_FLAGS & 0b1000) >> 3)
#define SET_ZERO_FLAG(b)        (ALU_FLAGS = (ALU_FLAGS & ~(0b1))       | ((b) != 0))
#define SET_SIGN_FLAG(b)        (ALU_FLAGS = (ALU_FLAGS & ~(0b1 << 1))  | ((b) != 0) << 1)
#define SET_OVERFLOW_FLAG(b)    (ALU_FLAGS = (ALU_FLAGS & ~(0b1 << 2))  | ((b) != 0) << 2)
#define SET_CARRY_FLAG(b)       (ALU_FLAGS = (ALU_FLAGS & ~(0b1 << 3))  | ((b) != 0) << 3)

extern uint8_t halted;
extern uint8_t interrupts_enabled;

extern uint16_t* ram;
extern uint32_t ram_sizew;

#define OPCODE_BIT_LENGTH 6
#define REG_BIT_LENGTH 3
#define OPCODE_MASK 0b00111111
#define REG_A_MASK  0b111000000
#define REG_B_MASK  0b111000000000

#define SPT_REG reg_file[6]
#define MEM_H_REG reg_file[7]

#endif