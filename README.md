# Tram Type-M Interface

![Bildbeschreibung]([URL-des-Bildes](https://www.strassenbahn-online.de/Betriebshof/Duewag/M/M_top.jpg)

*Quelle: [Link zur Bildquelle]([URL-der-Bildquelle](https://www.strassenbahn-online.de/Betriebshof/Duewag/M/M_top.jpg)*


## Initial Setup using PlatformIO

Follow the steps below to perform a fresh upload to an Arduino/Waveshare Nano ESP32-S3:

1. **Build and Flash Partition Binary**

   Flash the `partitions.bin` file to address `0x8000` using the following command:

   ```esptool.py --port $UPLOAD_PORT --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x8000 partitions.bin```

2. **Upload File System**

   Upload `littlefs.bin` to address `0x190000` (SPIFFS).

3. **Upload Firmware**

   Upload `firmware.bin` to address `0x10000` (factory).

## To-Do
- [ ] Evaluating the data packets for LokSim3D and setting the outputs accordingly
- [ ] Store Config in Header-Files instead of using Filesystem
