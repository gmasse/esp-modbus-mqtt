# esp-modbus-mqtt

#### Flashing firmware
```
cp .plateformio/packages/framework-arduinoespressif32/tools/sdk/bin/bootloader_dio_40m.bin .
cp .platformio/packages/framework-arduinoespressif32/tools/partitions/default.bin .

esptool.py --chip esp32 --port /dev/ttyUSB1 --baud 460800 \
 --before default_reset --after hard_reset write_flash -z \
 --flash_mode dio --flash_freq 40m --flash_size detect \
 0x1000 bootloader_dio_40m.bin 0x8000 default.bin 0x10000 firmware.bin
```
