# Tram Type-M Interface

## Initial Setup using PlatformIO

Follow the steps below to perform a fresh upload:

1. **Build and Flash Partitions**

   Flash the `partitions.bin` file to address `0x8000` using the following command:

   ```bash
   {path/to/esptool/}esptool.exe --port COM5 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x8000 partitions.bin
   ```

2. **Upload File System**

   Upload `littlefs.bin` to address `0x10000` (SPIFFS).

3. **Upload Firmware**

   Upload `firmware.bin` to address `0x810000` (factory).
