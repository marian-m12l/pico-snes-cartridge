#pragma once

#include <stdint.h>

uint8_t init_rom(const uint8_t* romdata, uint32_t size);
void loop_menu();
void loop_lorom();
void loop_hirom();
uint8_t* selected_rom();
