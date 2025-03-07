cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(pico-snes-cartridge C CXX ASM)

pico_sdk_init()


add_executable(pico-snes-cartridge
    main.c
    rom_helloworld.c
    #rom_color.c
    #rom_init32k.c
    #rom_bazz1_32k.c
    #rom_hello32k.c
    #rom_adventure04.c
    #rom_mrdo.c             # SlowROM LoROM 256KiB
    #rom_snes3d_demo.c      # SlowROM LoROM 256KiB
    #rom_smw.c              # SlowROM LoROM 512KiB
    #rom_fzero.c            # SlowROM LoROM 512KiB
    #rom_ff.c               # SlowROM LoROM 512KiB
    #rom_mspacman.c         # FastROM LoROM 256KiB
    #rom_hilda.c            # SlowROM LoROM 256KiB
    #rom_christmas_craze.c  # SlowROM LoROM 512KiB
    #rom_superrambler.c     # SlowROM LoROM 256KiB
    #rom_sureinstinct.c     # FastROM HiROM 512KiB
    #rom_incognity.c        # FastROM LoROM 512KiB
    #rom_supermariokart.c   # FastROM HiROM 512KiB + DSP-1
    #rom_smpspeed.c         # FastROM LoROM 32KiB
)

pico_generate_pio_header(pico-snes-cartridge ${CMAKE_CURRENT_LIST_DIR}/counter.pio)

target_compile_definitions(pico-snes-cartridge PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=44
  PICO_DEFAULT_UART_RX_PIN=45

  #PICO_STACK_SIZE=0x100
  #PICO_HEAP_SIZE=0
)

pico_set_binary_type(pico-snes-cartridge copy_to_ram)

pico_set_program_name(pico-snes-cartridge "pico-snes-cartridge")
pico_set_program_version(pico-snes-cartridge "0.1")

pico_enable_stdio_uart(pico-snes-cartridge 1)
pico_enable_stdio_usb(pico-snes-cartridge 0)

target_link_libraries(pico-snes-cartridge pico_stdlib pico_multicore hardware_pio hardware_dma hardware_xip_cache)

pico_add_extra_outputs(pico-snes-cartridge)
