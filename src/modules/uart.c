#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uart.h"
#include "module.h"

Module* uart_module;

const char sysuart_dev_name[] = "SYSUART";
#define SYSUART_DEV_NAME_SIZE (sizeof(sysuart_dev_name)-1)

UART_Dev* uart_devs;
int uart_devs_count;

uint8_t uart_buf[32];
uint8_t buf_ptr, buf_len;

// memmap:
// name length | name | uart data

uint16_t uart_read(uint32_t address, uint8_t is_peek) {
    // return the length of the device name/id
    if (address == uart_module->address) {
        return SYSUART_DEV_NAME_SIZE;
    }
    // return the device name/id
    if (address <= uart_module->address + (SYSUART_DEV_NAME_SIZE/2 + SYSUART_DEV_NAME_SIZE%2)) {
        uint8_t index = (address - uart_module->address - 1) * 2;
        return ((uint16_t)(sysuart_dev_name[index]) << 8) + (uint16_t)(sysuart_dev_name[index+1]);
    }

    if (buf_ptr < buf_len) {
        if (!is_peek)
            return uart_buf[buf_ptr++];
        else
            return uart_buf[buf_ptr];
    }
    else
        return 0;
}

void uart_write(uint32_t address, uint16_t data) {
    if (address <= uart_module->address + (SYSUART_DEV_NAME_SIZE/2 + SYSUART_DEV_NAME_SIZE%2)) {
        return;
    }

    for (int i = 0; i < uart_devs_count; i++) {
        uart_devs[i].receive((uint8_t)(data & 0xFF));
    }
}

void tick_uart_devs() {
    for (int i = 0; i < uart_devs_count; i++) {
        uart_devs[i].tick();
    }
}

void destroy_uart() {
    free(uart_devs);
}

void connect_uart_dev(void (*receive_func)(uint8_t data), void (*tick_func)()) {
    if (uart_devs == NULL) {
        uart_devs = (UART_Dev*) malloc(sizeof(UART_Dev));
        uart_devs_count = 1;
    }
    else {
        uart_devs = (UART_Dev*) realloc(uart_devs, (++uart_devs_count)*sizeof(UART_Dev));
    }

    uart_devs[uart_devs_count-1].receive = receive_func;
    uart_devs[uart_devs_count-1].tick = tick_func;
}

void uart_send(uint8_t* data, int datalen) {
    for (buf_ptr = 0; buf_ptr < datalen && buf_ptr < sizeof(uart_buf); buf_ptr++) {
        uart_buf[buf_ptr] = data[buf_ptr];
    }
    buf_len = buf_ptr+1;
    buf_ptr = 0;
    // TODO: request interrupt
    setHardwareInt(HWINT_SYSUART);
}

void mod_uart_init() {
    uart_module = (Module*) malloc(sizeof(Module));

    uart_module->address = 0x2000000 + 0x2000;
    uart_module->size = 1 + (SYSUART_DEV_NAME_SIZE/2 + SYSUART_DEV_NAME_SIZE%2) + 1;
    uart_module->dev_class = 0xfa;

    uart_module->read = &uart_read;
    uart_module->write = &uart_write;

    uart_module->name = (char*) malloc((SYSUART_DEV_NAME_SIZE+1)*sizeof(char));
    strncpy(uart_module->name, sysuart_dev_name, (SYSUART_DEV_NAME_SIZE+1));

    uart_module->moduleTick = &tick_uart_devs;

    uart_module->destroyModule = &destroy_uart;

    register_module(uart_module);

    // INIT THE UART DEVICES BELOW //
}