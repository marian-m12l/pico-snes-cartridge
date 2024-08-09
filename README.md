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
