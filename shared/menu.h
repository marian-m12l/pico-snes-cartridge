#pragma once

typedef struct {
    //char len;
    char name[21];
    void* address;
} rom_t;

typedef struct {
    char count;
    rom_t entries[22];
} roms_t;
