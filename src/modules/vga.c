#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <SDL2/SDL.h>

#include "arch.h"
#include "module.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

Module* vga_module;

const char vga_dev_name[] = "FB_OUT_DEV";
#define VGA_DEV_NAME_SIZE (sizeof(vga_dev_name)-1)
uint16_t mode;
uint32_t fb_address;

uint8_t screen_stop = 0;
pthread_t display_fb_tid;

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* screen_texture;

// memmap:
// name length | name | mode | fb address

uint16_t vga_read(uint32_t address, uint8_t is_peek) {
    // return the length of the device name/id
    if (address == vga_module->address) {
        return VGA_DEV_NAME_SIZE;
    }
    // return the device name/id
    if (address <= vga_module->address + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2)) {
        uint8_t index = (address - vga_module->address - 1) * 2;
        return ((uint16_t)(vga_dev_name[index]) << 8) + (uint16_t)(vga_dev_name[index+1]);
    }

    // return the mode of the VGA device
    if (address <= vga_module->address + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) + 1) {
        return mode;
    }
    // return the FB address
    if (address <= vga_module->address + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) + 1 + 2) {
        if (!(address - vga_module->address - 1 - (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) - 1)) {
            return fb_address & 0xFFFF;
        }
        return fb_address & 0xFFFF0000;
    }

    return 0;
}

void vga_write(uint32_t address, uint16_t data) {
    if (address <= vga_module->address + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2)) {
        return;
    }

    // set the mode of the VGA device
    if (address <= vga_module->address + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) + 1) {
        mode = data;
        return;
    }
    // set the FB address
    if (address <= vga_module->address + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) + 1 + 2) {
        if (!(address - vga_module->address - 1 - (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) - 1)) {
            fb_address = (fb_address & 0xFFFF0000) + data;
            return;
        }

        fb_address = (fb_address & 0xFFFF) + (data << 16);
        return;
    }
}

static void display_fb() {
    // Graphics init
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Could not initiate the window for VGA module! SDL Error: %s\n", SDL_GetError());
        return;
    }

    window = SDL_CreateWindow("PT16B00 CPU - Emulator - VGA Module",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    if (window == NULL) {
        printf("VGA module: Window could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("VGA module: Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    screen_texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
            SCREEN_WIDTH, SCREEN_HEIGHT);
    if (screen_texture == NULL) {
        printf("VGA module: Texture could not be created! SDL Error: %s\n", SDL_GetError());
        return;
    }

    uint16_t fb_data[SCREEN_HEIGHT][SCREEN_WIDTH];
    extern void sdl_kb_set_data(uint8_t* kb_scancodes);
    uint8_t last_1st_kb_scancode = 0;

    while (!screen_stop) {
        for (int y = 0; y < SCREEN_HEIGHT; y++) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                fb_data[y][x] = memread(fb_address + y*SCREEN_WIDTH + x);
            }
        }

        SDL_UpdateTexture(screen_texture, NULL, fb_data, SCREEN_WIDTH * 2); // `SCREEN_WIDTH * 2` is because of 16-bit colors
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_UpdateWindowSurface(window);

        uint8_t kb_scancodes[7] = {0,0,0,0,0,0,0};
        uint8_t cur_scancode_i = 1;
        for (SDL_Event e; SDL_PollEvent(&e);) {
            switch (e.type) {
                case SDL_KEYDOWN:
                    // set scancodes
                    if (cur_scancode_i > 6) break;
                    kb_scancodes[cur_scancode_i] = e.key.keysym.scancode;
                    // set modifiers
                    if (e.key.keysym.mod & KMOD_LCTRL)
                        kb_scancodes[0] |= 1;
                    if (e.key.keysym.mod & KMOD_LSHIFT)
                        kb_scancodes[0] |= 1 << 1;
                    if (e.key.keysym.mod & KMOD_LALT)
                        kb_scancodes[0] |= 1 << 2;
                    if (e.key.keysym.mod & KMOD_LGUI)
                        kb_scancodes[0] |= 1 << 3;
                    if (e.key.keysym.mod & KMOD_RCTRL)
                        kb_scancodes[0] |= 1 << 4;
                    if (e.key.keysym.mod & KMOD_RSHIFT)
                        kb_scancodes[0] |= 1 << 5;
                    if (e.key.keysym.mod & KMOD_RALT)
                        kb_scancodes[0] |= 1 << 6;
                    if (e.key.keysym.mod & KMOD_RGUI)
                        kb_scancodes[0] |= 1 << 7;
                    break;

                case SDL_QUIT:
                    screen_stop = 1;
                    break;

                default:
                    break;
            }
        }

        if (kb_scancodes[1] != 0 /*|| last_1st_kb_scancode != 0*/)
            sdl_kb_set_data(kb_scancodes);

        last_1st_kb_scancode = kb_scancodes[1];

        SDL_Delay(16); // 60 FPS
    }
}

void screen_tick() {
    if (screen_stop) {
        detach_module(vga_module);
    }
}

void destroy_screen() {
    screen_stop = 1;
    pthread_join(display_fb_tid, NULL);
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void mod_vga_init() {
    vga_module = (Module*) malloc(sizeof(Module));

    vga_module->address = 0x02000000 + 0x1000;
    vga_module->size = 1 + (VGA_DEV_NAME_SIZE/2 + VGA_DEV_NAME_SIZE%2) + 1 + 2;
    vga_module->dev_class = 0xaf;

    vga_module->read = &vga_read;
    vga_module->write = &vga_write;

    vga_module->name = (char*) malloc((VGA_DEV_NAME_SIZE+1)*sizeof(char));
    strncpy(vga_module->name, vga_dev_name, (VGA_DEV_NAME_SIZE+1));

    vga_module->moduleTick = &screen_tick;

    vga_module->destroyModule = &destroy_screen;

    register_module(vga_module);

    // Window init in the display_fb process, because SDL is not multithreading friendly...
    
    pthread_create(&display_fb_tid, NULL, (void* (*)(void *)) display_fb, NULL);
}