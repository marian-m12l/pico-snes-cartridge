cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(pico-snes-cartridge C CXX ASM)

pico_sdk_init()


add_executable(pico-snes-cartridge
    main.c
    #rom_helloworld.c
    rom_color.c
    #rom_init32k.c
    #rom_bazz1_32k.c
    #rom_hello32k.c
)

target_compile_definitions(pico-snes-cartridge PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=34
  PICO_DEFAULT_UART_RX_PIN=35
)

pico_set_binary_type(pico-snes-cartridge copy_to_ram)

pico_set_program_name(pico-snes-cartridge "pico-snes-cartridge")
pico_set_program_version(pico-snes-cartridge "0.1")

pico_enable_stdio_uart(pico-snes-cartridge 1)
pico_enable_stdio_usb(pico-snes-cartridge 0)

target_link_libraries(pico-snes-cartridge pico_stdlib)

pico_add_extra_outputs(pico-snes-cartridge)
