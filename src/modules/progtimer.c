#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include "module.h"
#include "utils.h"

Module* progtimer_module;

pthread_t progtimer_tid;
uint8_t stop_progtimer = 0;

const char progtimer_dev_name[] = "PROGTIMER";
#define PROGTIMER_DEV_NAME_SIZE (sizeof(progtimer_dev_name)-1)

const uint32_t progtimer_frequency = 1000000; // Hz

// State reg:
// bits:    |         6         |       4        |         6         |
// content: | one-shot/periodic | selected timer | start/stop timers |
uint16_t progtimer_state = 0;
#define PROGTIMER_STSTTIMERS_BITLEN 6
#define PROGTIMER_SELTIMER_BITLEN 4

// bit map of interrupted timers (bit index +2 gives the timer id)
uint16_t timerInts = 0;

uint32_t progtimer_settime_period[2];
uint64_t progtimer_settime_actual[2];

uint64_t progtimer_time;

// memmap:
// name length | name | freq (low 16b) | freq (high 16b) | settime (in ns; low 16b) | settime (in ns; high 16b) |
// | control (start/stop;one-shot/periodic) | other timers' interrupts
uint16_t progtimer_read(uint32_t address, uint8_t is_peek) {
    // return the length of the device name/id
    if (address == progtimer_module->address) {
        return PROGTIMER_DEV_NAME_SIZE;
    }
    // return the device name/id
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2)) {
        uint8_t index = (address - progtimer_module->address - 1) * 2;
        return ((uint16_t)(progtimer_dev_name[index]) << 8) + (uint16_t)(progtimer_dev_name[index+1]);
    }

    // return the frequency
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 1) {
        return progtimer_frequency & 0xFFFF;
    }
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2) {
        return (progtimer_frequency & 0xFFFF0000) >> 16;
    }
    // return the count time of selected timer
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 1) {
        if (((progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111) < 2)
            return progtimer_settime_period[(progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111] & 0xFFFF;
    }
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2) {
        if (((progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111) < 2)
            return (progtimer_settime_period[(progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111] & 0xFFFF0000) >> 16;
    }
    // return the state of the timers
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2 + 1) {
        return progtimer_state;
    }
    // return the state of the timers' interrupts
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2 + 1 + 1) {
        return timerInts;
    }

    return 0;
}

void progtimer_write(uint32_t address, uint16_t data) {
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2) {
        return;
    }

    // set the count time of selected timer
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 1) {
        if (((progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111) < 2) {
            progtimer_settime_period[(progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111]
                = (progtimer_settime_period[(progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111] & 0xFFFF0000) + data;
        }
        return;
    }
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2) {
        if (((progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111) < 2) {
            progtimer_settime_period[(progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111]
                = progtimer_settime_period[(progtimer_state >> PROGTIMER_STSTTIMERS_BITLEN) & 0b1111] + (data << 16);
        }
        return;
    }
    // set the state of the timers
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2 + 1) {
        for (int i = 0; i < PROGTIMER_STSTTIMERS_BITLEN; i++) {
            if ((progtimer_state & (1 << i))
                != (data & (1 << i))) {
                progtimer_settime_actual[i] = progtimer_time + progtimer_settime_period[i];
            }
        }

        progtimer_state = data;
        return;
    }
    // set the state of the timers' interrupts (used to clear it after being read)
    if (address <= progtimer_module->address + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2 + 1 + 1) {
        timerInts = data;
        return;
    }
}

static void process_progtimer() {
    // increase time
    struct timeval progtimer_st, progtimer_et;
    int deltaT = 0; // ns
    while(!stop_progtimer) {
        gettimeofday(&progtimer_st, NULL);

        progtimer_time += deltaT;
        for (int i = 0; i < sizeof(progtimer_settime_actual)/sizeof(progtimer_settime_actual[0]); i++) {
            if (progtimer_state & (1 << i)) {
                if (progtimer_settime_actual[i] > (progtimer_time-deltaT) && progtimer_settime_actual[i] <= progtimer_time) {
                    if (i < 2) {
                        setHardwareInt(i);
                    }
                    else {
                        timerInts |= 1 << (i-2);
                        setHardwareInt(3);
                    }

                    if ((progtimer_state >> (PROGTIMER_STSTTIMERS_BITLEN + PROGTIMER_SELTIMER_BITLEN)) & (1 << i)) {
                        progtimer_settime_actual[i] = progtimer_time + progtimer_settime_period[i];
                    }
                }
            }
        }

        gettimeofday(&progtimer_et, NULL);
        waitns((1000000000/progtimer_frequency) - ((progtimer_et.tv_sec - progtimer_st.tv_sec) * 1000000000 + (progtimer_et.tv_usec - progtimer_st.tv_usec) * 1000));

        gettimeofday(&progtimer_et, NULL);
        deltaT = (progtimer_et.tv_sec - progtimer_st.tv_sec) * 1000000000 + (progtimer_et.tv_usec - progtimer_st.tv_usec) * 1000;
    }
}

void destroy_progtimer() {
    stop_progtimer = 1;
    pthread_join(progtimer_tid, NULL);
}

void mod_progtimer_init() {
    progtimer_module = (Module*) malloc(sizeof(Module));

    progtimer_module->address = 0x2000000 + 0x3000;
    progtimer_module->size = 1 + (PROGTIMER_DEV_NAME_SIZE/2 + PROGTIMER_DEV_NAME_SIZE%2) + 2 + 2 + 1 + 1;
    progtimer_module->dev_class = 0xde;

    progtimer_module->read = &progtimer_read;
    progtimer_module->write = &progtimer_write;

    progtimer_module->name = (char*) malloc((PROGTIMER_DEV_NAME_SIZE+1)*sizeof(char));
    strncpy(progtimer_module->name, progtimer_dev_name, (PROGTIMER_DEV_NAME_SIZE+1));

    progtimer_module->moduleTick = NULL;

    progtimer_module->destroyModule = &destroy_progtimer;

    register_module(progtimer_module);

    pthread_create(&progtimer_tid, NULL, (void* (*)(void*)) process_progtimer, NULL);
}