# ESP32-P4-Nano 열감지 프린터 프로젝트

## 보드
- **Waveshare ESP32-P4-Nano**: ESP32-P4 (rev v1.0), RISC-V 듀얼코어, 16MB 플래시
- **MAC**: 30:ed:a0:e0:fc:fb
- **시리얼**: /dev/ttyACM0, 115200 보드레이트
- **WiFi**: ESP32-C6 코프로세서 통신 (`esp_hosted` + `esp_wifi_remote`)

## 개발 환경
- **ESP-IDF v5.4** at `~/projects/esp-idf-v5.4` (필수 - v6.1-dev는 칩 rev v3.1+ 요구)
- **타겟**: esp32p4
- **환경 설정**: `export IDF_PATH=~/projects/esp-idf-v5.4 && . $IDF_PATH/export.sh`
- **빌드**: `idf.py build`
- **플래싱**: `idf.py -p /dev/ttyACM0 flash`
- **모니터**: `stty -F /dev/ttyACM0 115200 raw -echo && timeout 12 cat /dev/ttyACM0`

## 프로젝트 구조
```
CMakeLists.txt              # 프로젝트 CMake (project: wifi_station)
sdkconfig.defaults          # 기본 설정 (WiFi, 파티션, 플래시)
partitions.csv              # 커스텀 파티션 (2MB factory)
.gitignore                  # build/, sdkconfig, managed_components/ 제외
main/
  CMakeLists.txt            # 현재 활성 소스 지정
  Kconfig.projbuild         # menuconfig 옵션
  idf_component.yml         # 의존성 (esp_wifi_remote, esp_hosted, usb)
  usb_printer_button_main.c # Phase 3.5: 버튼 + 프린터 제어 (현재 활성)
  i2s_es8311_example.c      # Phase #3: I2S 오디오
  mqtt_example_main.c       # Phase #2: MQTT 통신
  station_example_main.c    # Phase #1: WiFi 연결
  example_config.h          # GPIO 핀 설정
  logo.pbm                  # 임베드 PBM 이미지 (열감지 프린터용)
  canon.pcm                 # PCM 오디오 파일 (640KB)
```

## 완료된 기능
1. **Phase #1: HelloWorld** - ESP-IDF HelloWorld 빌드 및 플래싱 ✅
2. **Phase #2: WiFi Station** - ESP32-C6 코프로세서 통해 WiFi 연결 (IP: 192.168.1.195) ✅
3. **Phase #3: MQTT Pub/Sub** - myserver(192.168.1.158) mosquitto 양방향 통신 ✅
4. **Phase #4: I2S Audio** - ES8311 코덱 + NS4150B 앰프로 canon.pcm 재생 ✅
5. **Phase 3.5: 버튼 + 프린터** - GPIO 3 버튼으로 열감지 프린터 제어 ✅

## 주요 결정사항
- **WiFi 자격증명**: sdkconfig에 저장, .gitignore 처리 (SSID: showme_2.4G)
- **MQTT 브로커**: `ssh anton@myserver`에서 mosquitto 실행 중
- **파일 관리**: 기존 소스 삭제 금지 - 새 기능은 새 파일로 추가
- **파티션**: 2MB factory (바이너리 ~1.1MB, 46% 여유)
- **프린터**: Bixolon SRP-330II (80mm 열감지식, 180 DPI)

## GPIO 핀맵 (ESP32-P4-Nano)

### I2S 오디오
| 기능 | GPIO |
|------|------|
| I2C SCL | 8 |
| I2C SDA | 7 |
| I2S MCK | 13 |
| I2S BCK | 12 |
| I2S WS | 10 |
| I2S DOUT | 9 |
| I2S DIN | 11 |
| PA Enable | 53 |

### 버튼 및 프린터
| 기능 | GPIO/연결 |
|------|----------|
| 버튼 (마이크로 스위치) | GPIO 3 (풀업, 능동 로우) |
| 버튼 COM | GND |
| 버튼 NO | GPIO 3 |
| 열감지 프린터 | USB CDC ACM |

## 프린터 사양
- **모델**: Bixolon SRP-330II
- **용지 너비**: 80mm
- **인쇄 너비**: 72mm (최대)
- **해상도**: 180 DPI (7 dots/mm)
- **인터페이스**: USB CDC ACM
- **VID:PID**: 0x1504:0x006e
