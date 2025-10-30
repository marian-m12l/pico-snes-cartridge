# RP2350-based SNES cartridge

![launcher](https://github.com/user-attachments/assets/c68738c8-042b-4970-9e04-fa57992908f2)

https://github.com/user-attachments/assets/8309a93a-a673-430d-9ed4-e411bfe92a66

![snes_cart_1](https://github.com/user-attachments/assets/b732beac-30a8-45c6-bd4a-3e887160c2d5)
![snes_cart_2](https://github.com/user-attachments/assets/efc77e7f-af5f-476b-9c4b-7b1d2699faec)


## Pins

On rp2350 board (e.g. PGA2350):

- Address bus `A0`-`A23`: GPIO 0 (`A0`) to 23 (`A23`)
- Data bus `D0`-`D7`: GPIO 24 (`D0`) to 31 (`D7`)
- Cart pin `/CART`: GPIO 32
- Read pin `/RD`: GPIO 33
- Reset pin `/RESET`: GPIO 35
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

Add `rom.sfc` in slot `1`:
```
./tools/loadrom.sh rom.sfc 1"
```

Each slot (from 1 to 15) occupies 1MiB in flash memory.

# Running

```
$ openocd-rpi/installed/bin/openocd -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg
$ arm-none-eabi-gdb -ex 'target remote localhost:3333' -ex 'load' -ex 'monitor reset init' -ex 'continue' pico-snes-cartridge.elf
$ picocom -b 115200 /dev/ttyACM0 -g picocom.log
```

# Reset behaviour

Once you've selected a game, pressing the reset button on the SNES will restart this game.

A long-press (~1 second) will go back to the launcher.
