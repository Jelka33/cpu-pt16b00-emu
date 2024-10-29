#include <stdio.h>
#include <string.h>
#include "uart.h"

// Pass modifier keys and 6 scancodes!
void sdl_kb_set_data(uint8_t* kb_scancodes) {
    uint8_t uart_data[8] = {0x55}; // TODO: put UART keyboard command byte
    memcpy(&uart_data[1], kb_scancodes, 7);
    uart_send(uart_data, 8);
}