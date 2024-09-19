# Tram Type-M Interface

Interface with the purpose of operating Trains with the "THM-Simulator" and "LokSim3D" using the control panel of this tram:

![Bild](https://www.strassenbahn-online.de/Betriebshof/Duewag/M/M_top.jpg)

<sub>
<a href="https://www.strassenbahn-online.de/Betriebshof/Duewag/M/M_top.jpg">https://www.strassenbahn-online.de/Betriebshof/Duewag/M/M_top.jpg</a>
</sub>

## Initial Setup using PlatformIO

Follow the steps below to perform a fresh upload to an Arduino/Waveshare Nano ESP32-S3:

1. **Build Partition and Firmware Binary**

2. **Flash the `partitions.bin` file to address `0x8000` using the following command:**

   ```esptool.py --port $UPLOAD_PORT --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x8000 partitions.bin```

3. **Upload `firmware.bin` to address `0x10000` (factory)**
   
4. **Upload `littlefs.bin` to address `0x190000` (SPIFFS)**

To Upload to a "normal" ESP32 model, use the appropriate board in the platformio.ini file together with platformIO's integrated "Upload" and "upload filesystem" functions

## To-Do
- [ ] make use of the default ESP32-S3 partition (for some reason, the default "factory" part is to small)
- [ ] Evaluating the data packets from LokSim3D-Server and setting the outputs accordingly
- [ ] Store Config in Header-Files instead of using Filesystem
