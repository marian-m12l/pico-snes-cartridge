#!/bin/bash

OPENOCD=/home/marian/dev/openocd-rpi/installed/bin/openocd
rom=$1
index=$2    # Between 1 and 15

# List slots
if [ "$#" -eq 0 ]; then
    for i in {1..15}; do
        addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000)))
        output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 read_memory $addr 8 16" -c "exit;" 2>&1 | tail -1)
        magic=$(echo -n $output | xxd -r -p)
        if [ 'pico-snes-rom   ' = "$magic" ]; then
            addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x10)))
            output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 mdw $addr 1" -c "exit;" 2>&1 | tail -1)
            size=$(echo -n $((16#$(echo -n $output | cut -d' ' -f2))))
            size=$(($size / 1024))
            # First, try to read LoROM header @ 0x7fc0
            addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0x7fdc)))
            output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 mdh $addr 1" -c "exit;" 2>&1 | tail -1)
            complement=$(echo -n $((16#$(echo -n $output | cut -d' ' -f2))))
            addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0x7fde)))
            output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 mdh $addr 1" -c "exit;" 2>&1 | tail -1)
            checksum=$(echo -n $((16#$(echo -n $output | cut -d' ' -f2))))
            sum=$(($checksum + $complement))
            if [ 65535 -eq $sum ]; then
                addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0x7fc0)))
                output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 read_memory $addr 8 21" -c "exit;" 2>&1 | tail -1)
                name=$(echo -n $output | xxd -r -p)
            else
                # Then, try to read HiROM header @ 0xffc0
                addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0xffdc)))
                output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 mdh $addr 1" -c "exit;" 2>&1 | tail -1)
                complement=$(echo -n $((16#$(echo -n $output | cut -d' ' -f2))))
                addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0xffde)))
                output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 mdh $addr 1" -c "exit;" 2>&1 | tail -1)
                checksum=$(echo -n $((16#$(echo -n $output | cut -d' ' -f2))))
                sum=$(($checksum + $complement))
                if [ 65535 -eq $sum ]; then
                    addr=$(printf "0x%X" $((i * 0x100000 + 0x10000000 + 0x20 + 0xffc0)))
                    output=$($OPENOCD -f interface/jlink.cfg -c "transport select swd" -c "adapter speed 6000" -f target/rp2350.cfg -c "init; rp2350.dap.core0 read_memory $addr 8 21" -c "exit;" 2>&1 | tail -1)
                    name=$(echo -n $output | xxd -r -p)
                else
                    name="ROM ###"
                fi
            fi
            echo "Slot #$i occupied: $name ($size KiB)"
        else
            echo "Slot #$i free"
        fi
    done

    exit 0
fi

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
