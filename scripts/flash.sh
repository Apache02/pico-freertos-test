#!/bin/sh

if [ $# -eq 0 ]; then
  echo "Argument required"
  exit 1
fi

if [ ! -f $1 ]; then
  echo "File not found!"
  exit 1
fi

UF2_FILE="$1"
FLASH_TARGET="/media/$USER/RPI-RP2"

echo "  uf2 file: $UF2_FILE"
echo "  target: $FLASH_TARGET"
stat $UF2_FILE -c "  size: %s"

while [ ! -d /media/$USER/RPI-RP2 ]; do sleep .25; done

sleep 1
cp $UF2_FILE "$FLASH_TARGET/"
