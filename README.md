# Pico-based SNES cartridge

TODO

## Pins

On pico board:

- Address bus `A0`-`A15`: GPIO 0 (`A0`) to 15 (`A15`)
- Data bus `D0`-`D7`: GPIO 16 (`D0`) to 22 (`D6`) + GPIO 26 (`D7`)
- Read pin `/RD`: GPIO 27
- Cart pin `/CART`: GPIO 28

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
