#ifndef EMULATOR_UART_H
#define EMULATOR_UART_H

#include <stdint.h>

typedef struct UART_Dev {
    void (*receive)(uint8_t data);
    void (*tick)();
} UART_Dev;

void connect_uart_dev(void (*receive_func)(uint8_t data), void (*tick_func)());

void uart_send(uint8_t* data, int datalen);

// DECLARE ALL INIT FUNCTIONS OF DEVICES BELOW //
//   Naming rule: uart_<module_name>_init()    //

#endif