# Prebuilt firmware slot

Drop **`ble_bot.bin`** here for M5 OS manifest / SD install, or download a tagged release:

https://github.com/salvador-Data/BLE-Bot-Cardputer/releases

Build locally from [platformio/](../../platformio/README.md):

```bash
cd platformio
pio run -e m5stack-cardputer
cp .pio/build/m5stack-cardputer/firmware.bin ../data/firmware/ble_bot.bin
```

Binary artifacts are gitignored; publish via GitHub Releases after a hardware-verified build.
