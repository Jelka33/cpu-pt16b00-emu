#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include "arch.h"
#include "utils.h"
#include "module.h"

// define the global variables
uint16_t reg_file[8] = {0,0,0,0, 0,0,0,0,};
uint32_t pc = 0;
uint16_t spth = 0;
uint32_t ivtaddr = 0;
uint8_t ALU_FLAGS = 0;

uint8_t halted = 0;
uint8_t interrupts_enabled = 0;

uint16_t* ram = NULL;
uint32_t ram_sizew = 0;


void print_state_info();
void processInt(int intvec, uint32_t* next_pc);

int main(int argc, char** argv) {
    Args args;
    parse_cmd_args(argc, argv, &args);
    load_rom_to_mem(args.rom_path, 64 * pow(2, 20));
    init_mods();

    uint32_t next_pc = 0;
    uint8_t run = 0;
    uint8_t termoff = 0;
    uint32_t breakpoints[64];
    uint8_t cur_brpt = 0;
    //struct timeval t_before_ts, t_after_ts;
    while (pc < ram_sizew) {
        // simulate in 1ms timeslices because of timing (50,000 cycles * 20ns = 1ms)
        //gettimeofday(&t_before_ts, NULL);
        for (int clk_cycles_in_ms = 0; clk_cycles_in_ms < 128000; clk_cycles_in_ms++) {
            // check for breakpoints
            for (int i = 0; i < sizeof(breakpoints)/sizeof(breakpoints[0]); i++) {
                if (breakpoints[i] == 0) break;
                if (breakpoints[i] == pc) {
                    halted = 1;
                    termoff = 0;
                    printf("Reached breakpoint at %u\n", breakpoints[i]);
                    break;
                }
            }

            if (!termoff) {
                while (!run || halted) {
                    printf("> ");
                    char input_cmd[128];
                    fgets(input_cmd, sizeof(input_cmd), stdin);
                    strcpy(&input_cmd[strlen(input_cmd)-1], " ");   // add a space and null terminator
                                                                    // for the following code

                    if (strlen(input_cmd) == 1) {
                        continue;
                    }

                    char* input_arg;
                    input_arg = strtok(input_cmd, " ");

                    // check the input
                    if (!strcmp(input_arg, "state")) {
                        print_state_info();
                    }
                    else if (!strcmp(input_arg, "n") || !strcmp(input_arg, "next")) {
                        halted = 0;
                        break;
                    }
                    else if (!strcmp(input_arg, "r") || !strcmp(input_arg, "run")) {
                        halted = 0;
                        run = 1;
                        termoff = 1;
                        break;
                    }
                    else if (!strcmp(input_arg, "b") || !strcmp(input_arg, "break")) {
                        // get the address
                        input_arg = strtok(NULL, " ");
                        if (input_arg == NULL) {
                            printf("No address passed!\n");
                            continue;
                        }
                        uint32_t address;
                        if (input_arg[0] == 'x') {
                            address = strtol(&input_arg[1], NULL, 16);
                        }
                        else {
                            address = strtol(&input_arg[0], NULL, 10);
                        }

                        breakpoints[cur_brpt++] = address;
                    }
                    else if (!strcmp(input_arg, "rmbreak")) {
                        // get the address
                        input_arg = strtok(NULL, " ");
                        if (input_arg == NULL) {
                            printf("No address passed!\n");
                            continue;
                        }
                        uint32_t address;
                        if (input_arg[0] == 'x') {
                            address = strtol(&input_arg[1], NULL, 16);
                        }
                        else {
                            address = strtol(&input_arg[0], NULL, 10);
                        }

                        for (int i = 0; i < sizeof(breakpoints)/sizeof(breakpoints[0]); i++) {
                            if (breakpoints[i] != address) {
                                continue;
                            }

                            for (int j = i; j < sizeof(breakpoints)/sizeof(breakpoints[0]) - 1; j++) {
                                breakpoints[j] = breakpoints[j+1];
                            }

                            breakpoints[sizeof(breakpoints)/sizeof(breakpoints[0]) - 1] = 0;

                            break;
                        }
                    }
                    else if (!strcmp(input_arg, "breakpoints")) {
                        printf("Breakpoints at: ");
                        for (int i = 0; breakpoints[i] != 0; i++) {
                            printf("%u (0x%x), ", breakpoints[i], breakpoints[i]);
                        }
                        printf("\n");
                    }
                    else if (!strcmp(input_arg, "peekmem")) {
                        // get the address
                        input_arg = strtok(NULL, " ");
                        if (input_arg == NULL) {
                            printf("No address passed!\n");
                            continue;
                        }
                        uint32_t address;
                        if (input_arg[0] == 'x') {
                            address = strtol(&input_arg[1], NULL, 16);
                        }
                        else if (input_arg[0] == 'b') {
                            address = strtol(&input_arg[1], NULL, 2);
                        }
                        else {
                            address = strtol(&input_arg[0], NULL, 10);
                        }

                        // get the amount of data to print
                        input_arg = strtok(NULL, " ");
                        if (input_arg == NULL || strtol(input_arg, NULL, 10) == 0) {
                            printf("No peek length passed. Assuming 1\n");
                            input_arg = "1";
                        }

                        // print the memory data
                        char ascii[17];
                        ascii[16] = '\0';
                        uint32_t next_i;

                        // print the full rows
                        for (uint32_t i = 0; i < strtol(input_arg, NULL, 10)/8; i++) {
                            printf("%08x: ", address+i*8);

                            for (uint8_t j = 0; j < 8; j++) {
                                printf("%04x ", mempeek(address + i*8 + j));

                                if ((unsigned char)(mempeek(address + i*8 + j)/256) >= ' ' &&
                                    (unsigned char)(mempeek(address + i*8 + j)/256) <= '~') {
                                        ascii[j*2] = (unsigned char)(mempeek(address + i*8 + j)/256);
                                }
                                else {
                                    ascii[j*2] = '.';
                                }
                                if ((unsigned char)(mempeek(address + i*8 + j)%256) >= ' ' &&
                                    (unsigned char)(mempeek(address + i*8 + j)%256) <= '~') {
                                        ascii[j*2+1] = (unsigned char)(mempeek(address + i*8 + j)%256);
                                }
                                else {
                                    ascii[j*2+1] = '.';
                                }
                            }

                            printf(" %s\n", ascii);

                            next_i = i+1;
                        }
                        // print the last, partial row
                        if (strtol(input_arg, NULL, 10)%8 > 0) {
                            printf("%08x: ", address+next_i*8);
                            memset(ascii, ' ', 16);     // set ascii to just spaces
                            for (uint8_t j = 0; j < strtol(input_arg, NULL, 10)%8; j++) {
                                printf("%04x ", mempeek(address + next_i*8 + j));

                                if ((unsigned char)(mempeek(address + next_i*8 + j)/256) >= ' ' &&
                                    (unsigned char)(mempeek(address + next_i*8 + j)/256) <= '~') {
                                        ascii[j*2] = (unsigned char)(mempeek(address + next_i*8 + j)/256);
                                }
                                else {
                                    ascii[j*2] = '.';
                                }
                                if ((unsigned char)(mempeek(address + next_i*8 + j)%256) >= ' ' &&
                                    (unsigned char)(mempeek(address + next_i*8 + j)%256) <= '~') {
                                        ascii[j*2+1] = (unsigned char)(memread(address + next_i*8 + j)%256);
                                }
                                else {
                                    ascii[j*2+1] = '.';
                                }
                            }
                            
                            printf("%*s %s\n", (int)(5*(8-strtol(input_arg, NULL, 10)%8)), "", ascii);
                        }
                    }
                    else if (!strcmp(input_arg, "exit")) {
                        exit(0);
                    }
                    else {
                        printf("Command not found\n");
                        continue;
                    }
                }
            }

            if (!halted) {
                next_pc = pc+1;

                uint16_t instruction = memread(pc);
                uint16_t prev_reg_v = reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH];

                // extract the 6 lsb (OPCODE)
                switch (instruction & OPCODE_MASK) {
                    default:
                        break;
                    
                    // ADD %reg_a, %reg_b
                    case 0b1:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        += reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] < prev_reg_v);
                        break;
                    // ADD %reg_a, num
                    case 0b10:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] += memread(pc+1);
                        next_pc = pc+2;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] < prev_reg_v);
                        break;
                    // ADC %reg_a, %reg_b
                    case 0b11:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        += reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)] 
                        + CARRY_FLAG ? 1 : 0;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] < prev_reg_v);
                        break;
                    // ADC %reg_a, num
                    case 0b100:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] += memread(pc+1) + CARRY_FLAG ? 1 : 0;
                        next_pc = pc+2;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] < prev_reg_v);
                        break;
                    // SUB %reg_a, %reg_b
                    case 0b101:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        -= reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] > prev_reg_v);
                        break;
                    // SUB %reg_a, num
                    case 0b110:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] -= memread(pc+1);
                        next_pc = pc+2;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] > prev_reg_v);
                        break;
                    // SBC %reg_a, %reg_b
                    case 0b111:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        -= reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)] 
                        - CARRY_FLAG ? 1 : 0;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] > prev_reg_v);
                        break;
                    // SBC %reg_a, num
                    case 0b1000:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] -= memread(pc+1) - CARRY_FLAG ? 1 : 0;
                        next_pc = pc+2;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] > prev_reg_v);
                        break;
                    // MUL %reg_a, %reg_b
                    case 0b1001:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        *= reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] < prev_reg_v);
                        break;
                    // MUL %reg_a, num
                    case 0b1010:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] *= memread(pc+1);
                        next_pc = pc+2;

                        SET_ZERO_FLAG(!reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        SET_SIGN_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000);
                        SET_OVERFLOW_FLAG((reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] & 0b1000000000000000)
                                            != (prev_reg_v & 0b1000000000000000));
                        SET_CARRY_FLAG(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] < prev_reg_v);
                        break;

                    // AND %reg_a, %reg_b
                    case 0b1011:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        &= reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];
                        // TODO: set flags
                        break;
                    // AND %reg_a, num
                    case 0b1100:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] &= memread(pc+1);
                        next_pc = pc+2;
                        // TODO: set flags
                        break;
                    // OR %reg_a, %reg_b
                    case 0b1101:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        |= reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];
                        // TODO: set flags
                        break;
                    // OR %reg_a, num
                    case 0b1110:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] |= memread(pc+1);
                        next_pc = pc+2;
                        // TODO: set flags
                        break;
                    // XOR %reg_a, %reg_b
                    case 0b1111:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        ^= reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];
                        // TODO: set flags
                        break;
                    // XOR %reg_a, num
                    case 0b10000:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] ^= memread(pc+1);
                        next_pc = pc+2;
                        // TODO: set flags
                        break;
                    // SHR %reg_a
                    case 0b10001:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] >>= 1; // logical!
                        // TODO: set flags
                        break;
                    // SAR %reg_a
                    case 0b10010:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                        = (int16_t)(reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]) >> 1; // arithmetical!
                        // TODO: set flags
                        break;

                    // CMP %reg_a, %reg_b
                    case 0b10011:
                        {
                            uint16_t sub_res
                            = reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]
                            - reg_file[(instruction & REG_B_MASK) >> (OPCODE_BIT_LENGTH+REG_BIT_LENGTH)];

                            SET_ZERO_FLAG(!sub_res);
                            SET_SIGN_FLAG(sub_res & 0b1000000000000000);
                            SET_OVERFLOW_FLAG((sub_res & 0b1000000000000000) != (prev_reg_v & 0b1000000000000000));
                            SET_CARRY_FLAG(sub_res > prev_reg_v);
                        }
                        break;
                    // CMP %reg_a, num
                    case 0b10100:
                        {
                            uint16_t sub_res = reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] - memread(pc+1);
                            next_pc = pc+2;

                            SET_ZERO_FLAG(!sub_res);
                            SET_SIGN_FLAG(sub_res & 0b1000000000000000);
                            SET_OVERFLOW_FLAG((sub_res & 0b1000000000000000) != (prev_reg_v & 0b1000000000000000));
                            SET_CARRY_FLAG(sub_res > prev_reg_v);
                        }
                        break;

                    // LOAD %reg_a, address
                    case 0b10101:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] = memread((MEM_H_REG << 16) + memread(pc+1));
                        next_pc = pc+2;
                        break;
                    // WRITE1B address
                    case 0b10110:
                        memwrite((MEM_H_REG << 16) + memread(pc+1), (memread((MEM_H_REG << 16) + memread(pc+1)) & 0b1111111100000000)
                        + (reg_file[5] & 0b11111111));
                        next_pc = pc+2;
                        break;
                    // WRITE2B address
                    case 0b10111:
                        memwrite((MEM_H_REG << 16) + memread(pc+1), reg_file[5]);
                        next_pc = pc+2;
                        break;
                    // WRITE3B address
                    case 0b11000:
                        memwrite((MEM_H_REG << 16) + memread(pc+1), reg_file[5]);
                        memwrite((MEM_H_REG << 16) + memread(pc+1) + 1, (memread((MEM_H_REG << 16) + memread(pc+1) + 1) & 0b1111111100000000)
                        + (reg_file[4] & 0b11111111));
                        next_pc = pc+2;
                        break;
                    // WRITE4B address
                    case 0b11001:
                        memwrite((MEM_H_REG << 16) + memread(pc+1), reg_file[5]);
                        memwrite((MEM_H_REG << 16) + memread(pc+1) + 1, reg_file[4]);
                        next_pc = pc+2;
                        break;
                    // MOV %reg_a, %reg_b
                    case 0b11010:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] =
                            reg_file[(instruction & REG_B_MASK) >> (REG_BIT_LENGTH+OPCODE_BIT_LENGTH)];
                        break;
                    // MOV %reg_a, num
                    case 0b11011:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] = memread(pc+1);
                        next_pc = pc+2;
                        break;
                    // PUSH %reg_a
                    case 0b11100:
                        SPT_REG--;
                        memwrite((spth << 16) + SPT_REG, reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]);
                        break;
                    // PUSH num
                    case 0b11101:
                        SPT_REG--;
                        memwrite((spth << 16) + SPT_REG, memread(pc+1));
                        next_pc = pc+2;
                        break;
                    // POP %reg_a
                    case 0b11110:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] = memread((spth << 16) + SPT_REG);
                        SPT_REG++;
                        break;
                    // PUSHF
                    case 0b11111:
                        SPT_REG--;
                        memwrite((spth << 16) + SPT_REG, (uint16_t)ALU_FLAGS);
                        break;
                    // POPF
                    case 0b100000:
                        ALU_FLAGS = (uint8_t)memread((spth << 16) + SPT_REG);
                        SPT_REG++;
                        break;

                    // JMP address
                    case 0b100001:
                        next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        break;
                    // JZ address
                    case 0b100010:
                        if (ZERO_FLAG)
                            next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        else
                            next_pc = pc+3;
                        break;
                    // JNZ address
                    case 0b100011:
                        if (!ZERO_FLAG)
                            next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        else
                            next_pc = pc+3;
                        break;
                    // JG address
                    case 0b100100:
                        if (!ZERO_FLAG && OVERFLOW_FLAG == SIGN_FLAG)
                            next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        else
                            next_pc = pc+3;
                        break;
                    // JL address
                    case 0b100101:
                        if (SIGN_FLAG ^ OVERFLOW_FLAG)
                            next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        else
                            next_pc = pc+3;
                        break;
                    // JA address
                    case 0b100110:
                        if (!ZERO_FLAG && !CARRY_FLAG)
                            next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        else
                            next_pc = pc+3;
                        break;
                    // JB address
                    case 0b100111:
                        if (CARRY_FLAG)
                            next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        else
                            next_pc = pc+3;
                        break;
                    // SJMP address
                    case 0b101000:
                        {
                            // check if the 10-bit number is positive or negative
                            // and assign sign_extension to either upper 6 bits of 0s or 1s
                            uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                            next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                        }
                        break;
                    // SJZ address
                    case 0b101001:
                        if (ZERO_FLAG)
                            {
                                uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                                next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                            }
                        break;
                    // SJNZ address
                    case 0b101010:
                        if (!ZERO_FLAG)
                            {
                                uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                                next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                            }
                        break;
                    // SJG address
                    case 0b101011:
                        if (!ZERO_FLAG && OVERFLOW_FLAG == SIGN_FLAG)
                            {
                                uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                                next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                            }
                        break;
                    // SJL address
                    case 0b101100:
                        if (SIGN_FLAG ^ OVERFLOW_FLAG)
                            {
                                uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                                next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                            }
                        break;
                    // SJA address
                    case 0b101101:
                        if (!ZERO_FLAG && !CARRY_FLAG)
                            {
                                uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                                next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                            }
                        break;
                    // SJB address
                    case 0b101110:
                        if (CARRY_FLAG)
                            {
                                uint16_t sign_extension = (0x200 & (instruction >> OPCODE_BIT_LENGTH)) ? 0xFC00 : 0;
                                next_pc = pc + (int16_t)(sign_extension | (instruction >> OPCODE_BIT_LENGTH));
                            }
                        break;

                    // CALL address
                    case 0b101111:
                        memwrite((spth << 16) + --SPT_REG, (pc+3) & 0b11111111);
                        memwrite((spth << 16) + --SPT_REG, (pc+3) & 0b1111111100000000);
                        next_pc = (memread(pc+2) << 16) + memread(pc+1);
                        break;
                    // RET
                    case 0b110000:
                        next_pc = (memread((spth << 16) + SPT_REG++) << 16) + memread((spth << 16) + SPT_REG++);
                        break;
                    // INT interrupt_vector
                    case 0b110001:
                        processInt((uint8_t)(instruction >> OPCODE_BIT_LENGTH), &next_pc);
                        break;
                    // IRET
                    case 0b110010:
                        next_pc = (memread((spth << 16) + SPT_REG++) << 16) + memread((spth << 16) + SPT_REG++);
                        ALU_FLAGS = memread((spth << 16) + SPT_REG++);
                        interrupts_enabled = 1;
                        break;

                    // HLT
                    case 0b110011:
                        halted = 1;
                        // TODO: remove
                        // measure time frame
                        // struct timeval tnow;
                        // gettimeofday(&tnow, NULL);
                        // printf("Time at halt (at pc %u): %lds %ldus\n", pc, tnow.tv_sec, tnow.tv_usec);
                        break;

                    // SETSPTH %reg_a
                    case 0b110100:
                        spth = reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH];
                        break;
                    // GETSPTH %reg_a
                    case 0b110101:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] = spth;
                        break;
                    // SETIVT address
                    case 0b110110:
                        ivtaddr = (memread(pc+2) << 16) + memread(pc+1);
                        next_pc = pc+3;
                        interrupts_enabled = 1;
                        break;

                    // LOAD %reg_a, %reg_b
                    case 0b110111:
                        reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] = memread((MEM_H_REG << 16) +
                                                                            reg_file[((instruction & REG_B_MASK)
                                                                            >> (REG_BIT_LENGTH + OPCODE_BIT_LENGTH))]);
                        break;
                    // WRITE1B %reg_a
                    case 0b111000:
                        memwrite((MEM_H_REG << 16) + reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH],
                            (memread((MEM_H_REG << 16) +
                            reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH]) & 0b1111111100000000)
                            + (reg_file[5] & 0b11111111));
                        break;
                    // WRITE2B %reg_a
                    case 0b111001:
                        memwrite((MEM_H_REG << 16) + reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH],
                            reg_file[5]);
                        break;
                    // WRITE3B %reg_a
                    case 0b111010:
                        memwrite((MEM_H_REG << 16) + reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH],
                            reg_file[5]);
                        memwrite((MEM_H_REG << 16) + reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] + 1,
                            (memread((MEM_H_REG << 16) +
                            reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] + 1) & 0b1111111100000000)
                            + (reg_file[4] & 0b11111111));
                        break;
                    // WRITE4B %reg_a
                    case 0b111011:
                        memwrite((MEM_H_REG << 16) + reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH],
                            reg_file[5]);
                        memwrite((MEM_H_REG << 16) + reg_file[(instruction & REG_A_MASK) >> OPCODE_BIT_LENGTH] + 1,
                            reg_file[4]);
                        break;
                }

                // process the hardware interrupts
                if ((instruction & OPCODE_MASK) != 0b110001) {
                    // if the current instruction was not an interrupt; TODO: remove if processInt() disables ints
                    processInt(getHardwareInt(), &next_pc);
                }

                pc = next_pc;
            }
            else {
                // check for interrupts while halted
                processInt(getHardwareInt(), &next_pc);
                pc = next_pc;
            }
        }

        //gettimeofday(&t_after_ts, NULL);
        //waitns(2560000 - (t_after_ts.tv_sec-t_before_ts.tv_sec) * 1000000000 + (t_after_ts.tv_usec - t_before_ts.tv_usec) * 1000);
    }

    // printf("\nram dump:\n");
    // for (int i = 0; i < ram_sizew; i++) {
    //     printf("%x ", memread(i));
    // }
    // printf("\n");

    return 0;
}

void print_state_info() {
    printf("     %%r0              %%r1              %%r2              %%r3\n");
    printf("bin: ");
    for (int i = 0; i < 4; i++) {
        printf("%016b ", reg_file[i]);
    }
    printf("\nhex: ");
    for (int i = 0; i < 4; i++) {
        printf("            %04x ", reg_file[i]);
    }
    printf("\ndec: ");
    for (int i = 0; i < 4; i++) {
        printf("%16d ", reg_file[i]);
    }

    printf("\n     %%r4              %%r5              %%spt             %%memh\n");
    printf("bin: ");
    for (int i = 4; i < 8; i++) {
        printf("%016b ", reg_file[i]);
    }
    printf("\nhex: ");
    for (int i = 4; i < 8; i++) {
        printf("            %04x ", reg_file[i]);
    }
    printf("\ndec: ");
    for (int i = 4; i < 8; i++) {
        printf("%16d ", reg_file[i]);
    }
    printf("\nPC: %10u; SPTH: %10u; IVT: %10u"
            "\nFLAGS> ZERO: %1b SIGN: %1b OVERFLOW: %1b CARRY: %1b"
            "\nhalted: %1b; interrupts enabled: %1b; next instruction: %016b\n",
            pc, spth, ivtaddr,
            ZERO_FLAG, SIGN_FLAG, OVERFLOW_FLAG, CARRY_FLAG,
            halted, interrupts_enabled, memread(pc));
}

void processInt(int intvec, uint32_t* next_pc) {
    if (interrupts_enabled && intvec >= 0) {
        //printf("\nint: %d\n", intvec);
        memwrite((spth << 16) + --SPT_REG, ALU_FLAGS);
        memwrite((spth << 16) + --SPT_REG, (*next_pc) & 0xFFFF);
        memwrite((spth << 16) + --SPT_REG, ((*next_pc) & 0xFFFF0000) >> 16);

        *next_pc = (memread(ivtaddr + 2 * intvec + 1) << 16) + memread(ivtaddr + 2 * intvec);
        interrupts_enabled = 0;
        halted = 0;
    }
}