#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "arch.h"
#include "utils.h"

void readBEuint16(uint16_t* buffer, int n, FILE* fptr) {
    for (int i = 0; i < n; i++) {
        unsigned char num[2];
        fread(&num, 2, 1, fptr);
        buffer[i] = num[0] << 8 | num[1];
    }
}

int8_t load_rom_to_mem(char* path, uint32_t max_sizeb) {
    FILE* fptr = fopen(path, "rb");

    // get the file size
    fseek(fptr, 0, SEEK_END);
    uint32_t fsizeb = ftell(fptr)+1;
    // check if the ROM fits into the RAM
    if (fsizeb > max_sizeb) {
        fclose(fptr);
        return -1;
    }
    fseek(fptr, 0, SEEK_SET);

    // read the ROM
    ram = (uint16_t*) malloc(max_sizeb*sizeof(uint8_t));
    ram_sizew = max_sizeb / 2;
    readBEuint16(ram, ram_sizew, fptr);

    fclose(fptr);
    return 0;
}

int8_t parse_cmd_args(int argc, char** argv, Args* args) {
    const char* home_path = getenv("HOME");

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                if (!strcmp(argv[i], "--version")) {
                    printf(
                        "Jelka PT16B00-CPU Emulator\n"
                        "version 0.1.0\n"
                    );
                    exit(0);
                }
            }
        }
        else {
            if (argv[i][0] == '~') {
                args->rom_path = (char*) malloc((strlen(home_path) + strlen(argv[i]) * sizeof(char)));
                strcpy(args->rom_path, home_path);
                strcat(args->rom_path, argv[i]+1);
            }
            else {
                args->rom_path = argv[i];
            }
        }
    }

    return 0;
}

void waitms(uint32_t ms) {
    struct timespec timer;
    timer.tv_sec = ms / 1000;
    timer.tv_nsec = (ms % 1000) * 1000000;

    while (nanosleep(&timer, &timer) < 0) {
        if (errno != EINTR) break;
    }
}

void waitns(uint32_t ns) {
    struct timespec timer;
    timer.tv_sec = 0;
    timer.tv_nsec = ns;

    while (nanosleep(&timer, &timer) < 0) {
        if (errno != EINTR) break;
    }
}