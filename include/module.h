#ifndef EMULATOR_MODULE_H_
#define EMULATOR_MODULE_H_

#include <stdint.h>
#include <pthread.h>

typedef struct Module {
    uint32_t address;
    uint8_t size;
    uint8_t dev_class;

    uint16_t (*read)(uint32_t address, uint8_t is_peek);
    void (*write)(uint32_t address, uint16_t data);

    char* name;
    char* (*getinfo)();

    void (*moduleTick)();

    void (*destroyModule)();
} Module;

extern Module** modules;
extern int module_count;

void init_mods();
void register_module(Module* module);
void tick_modules();
void detach_module(Module* module);

uint16_t mempeek(uint32_t address);
uint16_t memread(uint32_t address);
void memwrite(uint32_t address, uint16_t data);

// Hardware interrupts
extern volatile uint32_t hardware_int_vec;
int getHardwareInt();
void setHardwareInt(int intnum);
extern pthread_mutex_t hwIntLock;
#define HWINT_SYSUART 0x02

// ARRAY OF MODULES' INIT FUNCTIONS //
//  FILL IT IN THE `MODULE.C` FILE  //
extern void (*modinit[])();
extern int modinit_count;

// DECLARE ALL INIT FUNCTIONS OF MODULES BELOW //
//    Naming rule: mod_<module_name>_init()    //

void mod_vga_init();
void mod_uart_init();
void mod_progtimer_init();

#endif