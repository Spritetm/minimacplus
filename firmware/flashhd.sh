#!/bin/bash

if [ -n "$1" ]; then
	echo "Usage: $0 /dev/ttyUSBx hard_disk_file.bin"
fi

PORT=/dev/ttyUSB1
FILE=hard_disk.bin
if [ -n "$1" ]; then PORT=$1; fi
if [ -n "$2" ]; then FILE=$2; fi

python $IDF_PATH/components/esptool_py/esptool/esptool.py --chip esp32 --port $PORT --baud $((921600/2)) --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size detect 0x120000 $FILE

