cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(pico-snes-cartridge C CXX ASM)

pico_sdk_init()


add_executable(pico-snes-cartridge
    main_new.c
    bus.c
    cic.c
    launcher.c
)

pico_generate_pio_header(pico-snes-cartridge ${CMAKE_CURRENT_LIST_DIR}/counter.pio)

target_compile_definitions(pico-snes-cartridge PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=44
  PICO_DEFAULT_UART_RX_PIN=45

  #ENABLE_UART=1

  ENABLE_BUS=1
  #NO_LOAD=1
  #LOAD_NO_BANKS=1
  #LOAD_BANKS_16K=1
  LOAD_BANKS_4K=1
  
  #ENABLE_CIC=1
  CIC_DEBUG=1
  CIC_REGION_PAL=1
)

#pico_set_binary_type(pico-snes-cartridge copy_to_ram)

pico_set_program_name(pico-snes-cartridge "pico-snes-cartridge")
pico_set_program_version(pico-snes-cartridge "0.1")

pico_enable_stdio_uart(pico-snes-cartridge 1)
pico_enable_stdio_usb(pico-snes-cartridge 0)

target_link_libraries(pico-snes-cartridge pico_stdlib pico_multicore hardware_pio hardware_dma hardware_xip_cache)

pico_add_extra_outputs(pico-snes-cartridge)
