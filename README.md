# ESP32-P4-Nano 열감지 프린터 제어 프로젝트

## 프로젝트 개요
Waveshare ESP32-P4-Nano 보드를 이용하여 Bixolon SRP-330II 열감지식 프린터를 제어하는 IoT 프로젝트입니다. 마이크로 스위치 버튼으로 프린터에 이미지를 출력할 수 있습니다.

## 주요 기능

### ✅ 완료된 Phase
| Phase | 기능 | 상태 |
|-------|------|------|
| #1 | ESP-IDF HelloWorld | ✅ 완료 |
| #2 | WiFi Station (ESP32-C6) | ✅ 완료 |
| #3 | MQTT Pub/Sub | ✅ 완료 |
| #4 | I2S Audio (ES8311) | ✅ 완료 |
| **3.5** | **버튼 + 열감지 프린터** | **✅ 완료** |

### Phase 3.5: 버튼 제어 프린터 출력
- **GPIO 3 마이크로 스위치**: 30ms 디바운스 처리
- **USB 열감지 프린터**: Bixolon SRP-330II (VID:0x1504, PID:0x006e)
- **이미지 출력**: PBM 형식, 1.33배 스케일 (72mm 최적화)
- **카운팅**: 버튼 누름 횟수 추적

## 하드웨어 사양

### 메인 보드
- **보드**: Waveshare ESP32-P4-Nano
- **프로세서**: ESP32-P4 (RISC-V 듀얼코어, 400MHz)
- **플래시**: 16MB 외부 + 2MB 내부
- **RAM**: ~606KB 가용
- **MAC**: 30:ed:a0:e0:fc:fb

### 주변 기기
| 기기 | 사양 | 연결 |
|------|------|------|
| **버튼** | 마이크로 스위치 | GPIO 3 |
| **프린터** | Bixolon SRP-330II | USB CDC ACM |
| **코덱** | ES8311 (오디오) | I2C/I2S |
| **앰프** | NS4150B | GPIO 53 |
| **WiFi** | ESP32-C6 코프로세서 | UART |

## 프린터 사양
- **모델**: Bixolon SRP-330II
- **용지 너비**: 80mm (표준)
- **실제 인쇄 너비**: 72mm (최대)
- **해상도**: 180 DPI (7 dots/mm)
- **인터페이스**: USB CDC ACM
- **인쇄 속도**: 220mm/sec

## 개발 환경

### 필수 소프트웨어
- **ESP-IDF**: v5.4 (at `~/projects/esp-idf-v5.4`)
  - ⚠️ v6.1-dev는 칩 revision v3.1+ 필요
- **Python**: 3.12.7+
- **시리얼 포트**: `/dev/ttyACM0`, 115200 baud

### 빠른 시작
```bash
# 환경 설정
export IDF_PATH=~/projects/esp-idf-v5.4
source $IDF_PATH/export.sh

# 빌드
idf.py build

# 플래싱
idf.py -p /dev/ttyACM0 flash

# 모니터링
stty -F /dev/ttyACM0 115200 raw -echo && timeout 15 cat /dev/ttyACM0
```

## 이미지 형식 가이드

### PBM (Portable Bitmap) 포맷
프린터는 **PBM P4 바이너리** 형식만 지원합니다:
- **색상**: 흑백 1비트 (검은색/흰색만)
- **너비**: 384 pixels 권장 (1.33배 확대 → 72mm)
- **해상도**: 180 DPI 기준

### 이미지 생성 (ImageMagick)
```bash
convert input.png -resize 384x -monochrome output.pbm
```

### 이미지 생성 (Python)
```python
from PIL import Image

img = Image.open('input.png')
img = img.resize((384, int(384 * img.height / img.width)))
img = img.convert('1')
img.save('output.pbm')
```

## 사용 방법

### 버튼으로 프린터 제어
1. **프린터 연결**: USB 포트에 연결
2. **기기 시작**: 전원 공급
3. **버튼 누르기**: GPIO 3 마이크로 스위치
   - 프린터에서 logo.pbm 이미지 출력

## 프로젝트 구조
```
.
├── CLAUDE.md                        # 개발 가이드 (한글)
├── README.md                        # 이 파일
├── main/
│   ├── usb_printer_button_main.c    # Phase 3.5 (현재)
│   ├── logo.pbm                     # 임베드 이미지
│   └── ...
└── build/                           # 빌드 아티팩트
```

## 참고 자료
- [ESP-IDF 공식 문서](https://docs.espressif.com/projects/esp-idf/)
- [Bixolon SRP-330II](https://www.bixolon.com/)
- [PBM 포맷](https://en.wikipedia.org/wiki/Netpbm)

---

**최종 업데이트**: 2026-04-24
**상태**: ✅ Phase 3.5 완료 및 테스트 완료
