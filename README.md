# esp-modbus-mqtt

A Modbus RTU (RS-485) to MQTT Gateway (based on ESP32)

## Circuit

Auto-switching UART-to-RS485 converter:
```

                   VCC ---------------+
                                      |
                              +-------x-------+
        (PIN27)    RXD <------| RX            |
                              |      UART    B|----------<> B
        (PIN26)    TXD ------>| TX    TO      |
ESP32                         |     RS-485    |     RS485 bus side
                              |               |
                              |              A|----------<> A
                              |               |
                              +-------x-------+
                                      |
                                     GND
```
Manual switching UART-to-RS485 converter:
```
                   VCC ---------------+
                                      |
                              +-------x-------+
        (PIN27)    RXD <------| R             |
                              |      UART    B|----------<> B
        (PIN26)    TXD ------>| D     TO      |
ESP32                         |     RS-485    |     RS485 bus side
        (PIN25)    RTS --+--->| DE            |
                         |    |              A|----------<> A
                         +----| /RE           |
                              +-------x-------+
                                      |
                                     GND
```
NB: ESP32 pins are configurable at compilation time.

## TODO
- [ ] Configuration (Wifi credentials) Reset
- [ ] Factory Firmware
- [ ] Secure Boot

## FAQ
#### Passing environment variables via VS Code
Edit `.vscode/settings.json` and add the following lines:
```
  "terminal.integrated.env.osx": {
    "PIO_FIRMWARE_URL": "https://url/firmware.bin"
  },
```

#### Flashing firmware
```
cp .plateformio/packages/framework-arduinoespressif32/tools/sdk/bin/bootloader_dio_40m.bin .
cp .platformio/packages/framework-arduinoespressif32/tools/partitions/default.bin .

esptool.py --chip esp32 --port /dev/ttyUSB1 --baud 460800 \
 --before default_reset --after hard_reset write_flash -z \
 --flash_mode dio --flash_freq 40m --flash_size detect \
 0x1000 bootloader_dio_40m.bin 0x8000 default.bin 0x10000 firmware.bin
```
