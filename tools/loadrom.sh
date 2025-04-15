#!/bin/bash

OPENOCD=/home/marian/dev/openocd-rpi/installed/bin/openocd
rom=$1
index=$2    # Between 1 and 15

# Magic bytes
echo -en 'pico-snes-rom   ' > /tmp/rom.bin

# Rom size
size=$(stat -c "%s" "$rom")
echo $size
size=$(printf "0x%X" $((size)))
echo $size
printf "0: %.4x" $((size & 0xffff)) | xxd -r -g0 | dd conv=swab >> /tmp/rom.bin
printf "0: %.4x" $((size >> 16)) | xxd -r -g0 | dd conv=swab >> /tmp/rom.bin

# Padding
echo -en '\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff' >> /tmp/rom.bin

# Checksum
cksum -a bsd --raw "$rom" | dd conv=swab >> /tmp/rom.bin
cat "$rom" >> /tmp/rom.bin

# Destination address
addr=$(printf "0x%X" $((index * 0x100000 + 0x10000000)))

echo "programming rom $1 at address $addr"

$OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "program /tmp/rom.bin exit $addr"
