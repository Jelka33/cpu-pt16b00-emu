#ifndef EMULATOR_UTILS_H_
#define EMULATOR_UTILS_H_

#include <stdint.h>

int8_t load_rom_to_mem(char path[], uint32_t max_sizeb);

typedef struct Args {
    char *rom_path;
} Args;

int8_t parse_cmd_args(int argc, char** argv, Args* args);

void waitms(uint32_t ms);
void waitns(uint32_t ns);

#endif