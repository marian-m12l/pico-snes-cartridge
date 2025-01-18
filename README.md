# RP2350-based SNES cartridge

TODO

## Pins

On rp2350 board (e.g. PGA2350):

- Address bus `A0`-`A23`: GPIO 0 (`A0`) to 23 (`A23`)
- Data bus `D0`-`D7`: GPIO 24 (`D0`) to 31 (`D7`)
- Read pin `/CART`: GPIO 32
- Cart pin `/RD`: GPIO 33

- UART pins `TX` and `RX`: GPIO 44 and 45

## Build instructions

```
mkdir build
cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk-2.0.0 ..
make
```

# Running

```
$ openocd-rpi/installed/bin/openocd -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg
$ arm-none-eabi-gdb -ex 'target remote localhost:3333' -ex 'load' -ex 'monitor reset init' -ex 'continue' pico-snes-cartridge.elf
$ picocom -b 115200 /dev/ttyACM0 -g picocom.log
```
