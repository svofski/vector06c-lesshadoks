gobjcopy -I ihex disk.hex -O binary disk.bin
hexdump -v -e '/1 "%02X "' disk.bin >disk.hax
