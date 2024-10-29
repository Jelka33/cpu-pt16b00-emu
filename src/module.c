#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "module.h"
#include "arch.h"
#include "utils.h"

// FILL THE ARRAY WITH MODULES' INIT FUNCTIONS //
// All the functions in the array will be      //
// called and modules initialized              //
void (*modinit[])() = {
    mod_vga_init,
    mod_uart_init,
    mod_progtimer_init,
};
int modinit_count = 3;


Module** modules = NULL;
int module_count = 0;

// Every device will get 3 fields assigned:
//  - lower 16 bits of the address
//  - higher 16 bits of the address
//  - upper 8 bits are the device class and lower 8 bits
//      are the number of addresses that belong to the device
uint16_t* devicesAddrRom;
uint16_t devAddrRomLength;

pthread_t modules_tick_tid;

volatile uint32_t hardware_int_vec = 0;
pthread_mutex_t hwIntLock;

void init_mods() {
    pthread_mutex_init(&hwIntLock, NULL);

    for (int i = 0; i < modinit_count; i++) {
        (*modinit[i])();
        if (modules[module_count-1]->address >= ram_sizew)
            devAddrRomLength += 3;
    }

    devicesAddrRom = (uint16_t*) malloc(sizeof(uint16_t) * (devAddrRomLength+1));
    devicesAddrRom[0] = devAddrRomLength;
    for (int i = 0, j = 1; i < module_count; i++) {
        if (modules[i]->address >= ram_sizew) {
            devicesAddrRom[j++] = modules[i]->address & 0xFFFF;
            devicesAddrRom[j++] = modules[i]->address >> 16;
            devicesAddrRom[j++] = (modules[i]->dev_class << 8) + modules[i]->size;
        }
    }

    pthread_create(&modules_tick_tid, NULL, (void* (*)(void*)) tick_modules, NULL);
}

void register_module(Module* module) {
    if (modules == NULL) {
        modules = (Module**) malloc(sizeof(Module*));
        modules[0] = (Module*) malloc(sizeof(Module));
        module_count = 1;
    }
    else {
        modules = (Module**) realloc(modules, (++module_count)*sizeof(Module*));
        modules[module_count-1] = (Module*) malloc(sizeof(Module));
    }

    modules[module_count-1] = module;
}

void tick_modules() {
    for (;;) {
        for (int i = 0; i < module_count; i++) {
            if (modules[i]->moduleTick) {
                modules[i]->moduleTick();
            }
        }

        waitms(16);
    }
}

void detach_module(Module* module) {
    if (module == NULL)
        return;
    
    if (module->destroyModule) {
        module->destroyModule();
    }
    free(module);

    // Move all module pointers after this one by one place downwards
    int i = -1;
    for (; i < module_count; i++) {
        if (modules[i] == module)
            break;
    }
    if (i >= 0) {
        for (int j = i+1; j < module_count; j++) {
            modules[i++] = modules[j];
        }
        modules = (Module**) realloc(modules, (--module_count)*sizeof(Module*));
    }
    else {
        // TODO: remove this if it works :)
        printf("HOW IS A MODULE DETACHED BUT NO POINTER FOUND???\n");
    }
}

uint16_t mempeek(uint32_t address) {
    // If RAM address, read from it
    if (address < ram_sizew) {
        return ram[address];
    }

    // If address of devices address ROM, read from it
    if (address < 0x2000000+devAddrRomLength+1) {
        return devicesAddrRom[address-0x2000000];
    }

    // Search for the module to which the address belongs
    for (int i = 0; i < module_count; i++) {
        if ((address >= modules[i]->address) && (address < (modules[i]->address + modules[i]->size))) {
            return modules[i]->read(address, 1);
        }
    }

    return 0;
}

uint16_t memread(uint32_t address) {
    // If RAM address, read from it
    if (address < ram_sizew) {
        return ram[address];
    }

    // If address of devices address ROM, read from it
    if (address < 0x2000000+devAddrRomLength+1) {
        return devicesAddrRom[address-0x2000000];
    }

    // Search for the module to which the address belongs
    for (int i = 0; i < module_count; i++) {
        if ((address >= modules[i]->address) && (address < (modules[i]->address + modules[i]->size))) {
            return modules[i]->read(address, 0);
        }
    }

    return 0;
}

void memwrite(uint32_t address, uint16_t data) {
    // If RAM address, write to it
    if (address < ram_sizew) {
        ram[address] = data;
    }

    // If address of devices address ROM, ignore it because it's a ROM
    if (address < 0x2000000+devAddrRomLength+1) {
        return;
    }

    // Search for the module to which the address belongs
    for (int i = 0; i < module_count; i++) {
        if ((address >= modules[i]->address) && (address < (modules[i]->address + modules[i]->size))) {
            modules[i]->write(address, data);
        }
    }
}

int getHardwareInt() {
    pthread_mutex_lock(&hwIntLock);
    for (int i = 0; i < 32; i++) {
        if (hardware_int_vec & (1 << i)) {      // check for set bit
            hardware_int_vec = hardware_int_vec ^ (1 << i);     // clear the interrupt
            pthread_mutex_unlock(&hwIntLock);
            return i;
        }
    }

    pthread_mutex_unlock(&hwIntLock);
    return -1;
}

void setHardwareInt(int intnum) {
    pthread_mutex_lock(&hwIntLock);
    hardware_int_vec = hardware_int_vec | (1 << intnum);
    pthread_mutex_unlock(&hwIntLock);
}