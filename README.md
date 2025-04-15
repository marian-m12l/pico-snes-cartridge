# RP2350-based SNES cartridge

TODO

## Pins

On rp2350 board (e.g. PGA2350):

- Address bus `A0`-`A23`: GPIO 0 (`A0`) to 23 (`A23`)
- Data bus `D0`-`D7`: GPIO 24 (`D0`) to 31 (`D7`)
- Cart pin `/CART`: GPIO 32
- Read pin `/RD`: GPIO 33
- CIC pin 24 `D1`: GPIO 37
- CIC pin 55 `D2`: GPIO 38
- CIC pin 56 `CLK`: GPIO 39
- CIC pin 25 `RST`: GPIO 40

- UART pins `TX` and `RX`: GPIO 44 and 45

## Build instructions

```
mkdir build
cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk-2.1.0 -DPICO_BOARD=pimoroni_pga2350 ..
make
```

# Adding ROMs

Over SWD/JTAG using openocd:

```
openocd-rpi/installed/bin/openocd -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "program rom.sfc exit 0x10100000"
```

Over USB using picotool:

```
picotool load rom.sfc -o 0x10100000
```

# Running

```
$ openocd-rpi/installed/bin/openocd -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg
$ arm-none-eabi-gdb -ex 'target remote localhost:3333' -ex 'load' -ex 'monitor reset init' -ex 'continue' pico-snes-cartridge.elf
$ picocom -b 115200 /dev/ttyACM0 -g picocom.log
```
