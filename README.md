# Tram Type-M Interface

### To Upload to new Device using PlatformIO: 

1. Build and Flash partitions.bin to 0x8000:   
```esptool.exe --port COM5 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 16MB 0x8000  partitions.bin```

2. Upload firmware.bin to 0x810000 (factory)

3. Upload littlefs.bin to 0x10000  (spiffs)
