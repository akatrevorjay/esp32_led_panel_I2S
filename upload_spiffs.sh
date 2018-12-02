#!/bin/zsh
set -eo pipefail
set -xv

mkspiffs -c data -b 4096 -p 256 -s 2097152 spiffs.bin
esptool --baud 921600 write_flash 0x180000 spiffs.bin


