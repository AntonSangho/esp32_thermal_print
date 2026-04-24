# ESP32-P4-Nano Project

## Board
- **Waveshare ESP32-P4-Nano**: ESP32-P4 (rev v1.0), RISC-V dual-core, 16MB flash
- **MAC**: 30:ed:a0:e0:fc:fb
- **Serial**: /dev/ttyACM0, baud 115200
- WiFi via ESP32-C6 coprocessor (`esp_hosted` + `esp_wifi_remote`)

## Development Environment
- **ESP-IDF v5.4** at `~/projects/esp-idf-v5.4` (필수 - v6.1-dev는 chip rev v3.1+ 요구하므로 사용 불가)
- **Target**: esp32p4
- **Source env**: `export IDF_PATH=~/projects/esp-idf-v5.4 && . $IDF_PATH/export.sh`
- **Build**: `idf.py build`
- **Flash**: `idf.py -p /dev/ttyACM0 flash`
- **Monitor**: `stty -F /dev/ttyACM0 115200 raw -echo && timeout 12 cat /dev/ttyACM0`

## Project Structure
```
CMakeLists.txt              # 프로젝트 CMake (project: wifi_station)
sdkconfig.defaults          # 기본 설정 (WiFi, 파티션, 플래시 크기)
partitions.csv              # 커스텀 파티션 (2MB factory)
.gitignore                  # build/, sdkconfig, managed_components/ 등 제외
main/
  CMakeLists.txt            # 현재 활성 소스 지정
  Kconfig.projbuild         # menuconfig 옵션 (WiFi, MQTT, I2S)
  idf_component.yml         # 의존성 (esp_wifi_remote, esp_hosted, es8311)
  station_example_main.c    # #2 WiFi station 소스
  mqtt_example_main.c       # #3 MQTT + WiFi 통합 소스
  i2s_es8311_example.c      # #4 I2S 오디오 소스 (현재 활성)
  example_config.h          # I2S/I2C GPIO 핀 설정
  canon.pcm                 # 내장 PCM 오디오 파일 (640KB)
```

## Completed Issues
1. **HelloWorld** - ESP-IDF HelloWorld 빌드 및 플래싱 성공
2. **WiFi Station** - ESP32-C6 코프로세서 통해 WiFi 연결 (IP: 192.168.1.195)
3. **MQTT Pub/Sub** - myserver(192.168.1.158) mosquitto와 양방향 통신 성공
4. **I2S Audio** - ES8311 코덱 + NS4150B 앰프로 canon.pcm 재생 성공

## Key Decisions
- WiFi 크리덴셜 (SSID: showme_2.4G)은 sdkconfig에 저장, gitignore 처리
- MQTT 브로커: `ssh anton@myserver`에서 mosquitto 실행 중
- 기존 소스 파일 삭제 금지 - 새 기능은 새 파일로 추가
- 파티션: 2MB factory (바이너리 ~1.1MB, 46% 여유)

## I2S Audio Pin Map (ESP32-P4-Nano)
| Function | GPIO |
|----------|------|
| I2C SCL  | 8    |
| I2C SDA  | 7    |
| I2S MCK  | 13   |
| I2S BCK  | 12   |
| I2S WS   | 10   |
| I2S DOUT | 9    |
| I2S DIN  | 11   |
| PA Enable| 53   |
