export PATH="$HOME/.platformio/penv/bin:$PATH"
cd ../ESP32_Firmware/
pio run -e ac_ecu
pio run -e lights_ecu
pio run -e heating_ecu
pio run -e door_ecu
cp .pio/build/ac_ecu/firmware.bin ../WebApp/ota_update/ac_ecu
cp .pio/build/door_ecu/firmware.bin ../WebApp/ota_update/door_ecu
cp .pio/build/lights_ecu/firmware.bin ../WebApp/ota_update/lights_ecu
cp .pio/build/heating_ecu/firmware.bin ../WebApp/ota_update/heating_ecu