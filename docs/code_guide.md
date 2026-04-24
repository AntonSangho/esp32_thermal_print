# ESP32-P4-Nano 열감지 프린터 코드 흐름 가이드

## 📌 개요
이 문서는 `main/usb_printer_button_main.c`의 **전체 실행 흐름**을 단계별로 설명합니다.
기능을 추가하거나 수정할 때 참고하세요.

---

## 1️⃣ 전체 실행 흐름 (High-Level)

### app_main() 초기화 단계
1. **USB 호스트 스택 초기화**
   - `usb_host_install(&host_config)`
   - USB 버스 기본 구성 설정

2. **USB CDC ACM 드라이버 설치**
   - `cdc_acm_host_install(NULL)`
   - 프린터와 통신할 드라이버 등록

3. **세 개의 FreeRTOS 태스크 생성**
   - `usb_lib_task` (우선순위 20) ← **최우선**
   - `printer_task` (우선순위 5)
   - `button_task` (우선순위 10)

4. **app_main() 반환**
   - 태스크들이 백그라운드에서 평행 실행 시작

---

### 실행 중인 세 개 태스크

#### 🔵 usb_lib_task (우선순위 20)
**역할**: USB 이벤트 처리 무한 루프 (가장 중요!)

```c
while (1) {
    usb_host_lib_handle_events(100ms);
}
```

- 100ms 주기로 USB 버스 이벤트 확인
- 연결/해제, 데이터 송수신 이벤트 처리
- **⚠️ 이 태스크 없으면 프린터 전혀 작동 안 함**

---

#### 🟡 printer_task (우선순위 5)
**역할**: 프린터 연결 관리 & 출력 제어

```c
while (1) {
    1초마다 cdc_acm_host_open() 시도
    ├─ 연결 성공 → cdc_dev 핸들 저장 (유지)
    └─ 연결 실패 → 다시 시도
    대기 (1초)
}
```

- USB 프린터(VID:0x1504, PID:0x006e) 연결 폴링
- 한 번 연결되면 계속 유지
- `cdc_dev` 전역 변수가 NULL이 아니면 연결됨

---

#### 🟢 button_task (우선순위 10)
**역할**: GPIO 3 버튼 감지 & 디바운싱

```c
while (1) {
    GPIO 3 레벨 읽기 (10ms 주기)
    ├─ 100ms 동안 안정화 확인
    └─ LOW 상태 확정 → 이미지 출력
    
    500ms 대기 (재입력 방지)
}
```

- 마이크로 스위치 버튼의 눌림 감지
- 30개 샘플(10ms × 10) = 100ms 디바운싱
- 프린터 연결 시에만 출력

---

## 2️⃣ 각 태스크 상세 설명

### 🔵 usb_lib_task (우선순위 20)
```c
static void usb_lib_task(void *arg)
{
    while (1) {
        usb_host_lib_handle_events(100, NULL);
    }
    vTaskDelete(NULL);
}
```

**역할**: USB 버스의 연결/해제, 데이터 송수신 같은 **하드웨어 이벤트를 처리**합니다.

**동작 원리**:
1. `usb_host_lib_handle_events(100ms)` — 100ms 타임아웃으로 USB 이벤트 폴링
2. 이벤트 발생 시 → CDC ACM 드라이버가 등록한 콜백 실행
3. 100ms 동안 이벤트 없으면 반환 → 루프 다시 시작

**중요**: 이 태스크가 없으면 `cdc_acm_host_open()`, `cdc_acm_host_data_tx_blocking()`이 **무한 대기**에 빠집니다.

---

### 🟡 printer_task (우선순위 5)
```c
static void printer_task(void *arg)
{
    cdc_acm_host_device_config_t dev_config = {
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = NULL,
        .user_arg = NULL,
    };

    int poll_count = 0;

    while (1) {
        if (!cdc_dev) {  // ◄ 연결되지 않음 상태
            poll_count++;
            // VID=0x1504, PID=0x006e (Bixolon SRP-330II)
            esp_err_t ret = cdc_acm_host_open(PRINTER_VID, PRINTER_PID, 
                                               PRINTER_IFACE, &dev_config, &cdc_dev);
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "✓ Printer CONNECTED!");
            } else if (ret == ESP_ERR_NOT_FOUND && poll_count % 10 == 0) {
                ESP_LOGW(TAG, "Printer not found (poll #%d)", poll_count);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  // 1초 대기
    }
}
```

**역할**: **프린터와의 USB 연결을 관리**합니다.

**동작 원리**:
1. `cdc_dev == NULL` 이면 → `cdc_acm_host_open()` 호출 (1초마다 시도)
2. 연결 성공 → `cdc_dev` 핸들 저장 (전역 변수)
3. `cdc_dev != NULL` 상태가 계속 유지됨
4. 다른 태스크에서 `cdc_acm_host_data_tx_blocking()`으로 데이터 송신 가능

**특이점**: 연결 상태를 확인하기 위해 `cdc_acm_host_close()` 호출 로직이 **없음**
→ 따라서 한 번 연결되면 계속 연결 상태 유지 ✅

---

### 🟢 button_task (우선순위 10)
```c
static void button_task(void *arg)
{
    // GPIO 3을 풀업 입력으로 설정
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),    // GPIO_NUM_3
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,        // 풀업 활성화
    };
    gpio_config(&io_conf);

    uint32_t prev_state = gpio_get_level(BUTTON_PIN);
    uint32_t stable_count = 0;
    const uint32_t debounce_threshold = 10;      // 100ms (10 × 10ms)

    while (1) {
        uint32_t curr_state = gpio_get_level(BUTTON_PIN);

        // 상태 안정화 카운팅
        if (curr_state == prev_state) {
            stable_count++;
        } else {
            stable_count = 0;
            prev_state = curr_state;
        }

        // 100ms 동안 LOW 상태 유지되면 버튼 누름 확정
        if (stable_count >= debounce_threshold && curr_state == 0) {
            if (cdc_dev) {
                press_count++;
                parse_pbm_and_print();  // ◄ 이미지 출력!
            }

            stable_count = 0;
            vTaskDelay(pdMS_TO_TICKS(500));  // 500ms 대기 (재입력 방지)
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 10ms 주기로 샘플링
    }
}
```

**역할**: **GPIO 3 마이크로 스위치의 누름을 감지**하고 이미지를 출력합니다.

**동작 원리 (디바운싱 알고리즘)**:
```
시간 →
GPIO: 1 1 1 1 0 0 0 0 0 0 0 0 1 1 1
      ↑ 안정↑       ↑ 카운팅 중... (10회) ↑ 버튼 누름 확정!
      prev_state    stable_count++
```

1. **10ms 주기로** GPIO 3 레벨 읽음 (0 = LOW, 1 = HIGH)
2. **같은 상태가 10회 연속** → 100ms 안정화 확인
3. **LOW가 확정되면** → `press_count++` (카운팅)
4. **`parse_pbm_and_print()` 호출** → 이미지 출력 시작
5. **500ms 대기** → 버튼 바운스 및 재입력 방지

**GPIO 상태**:
- **HIGH (풀업)**: 버튼 미누름
- **LOW (GND 연결)**: 버튼 누름

---

## 3️⃣ 이미지 출력 흐름 상세

### 함수: `parse_pbm_and_print()`

```
parse_pbm_and_print()
    │
    ├─ [단계 1] 프린터 연결 확인
    │   if (!cdc_dev) return;
    │
    ├─ [단계 2] PBM 파일 헤더 파싱
    │   ├─ P4 매직넘버 확인
    │   ├─ width 파싱 (예: 384)
    │   ├─ height 파싱 (예: 799)
    │   └─ 픽셀 데이터 포인터 저장
    │
    ├─ [단계 3] 이미지 스케일링 (1.33배)
    │   ├─ malloc() 으로 스케일 버퍼 할당
    │   ├─ 각 원본 픽셀을 확대하여 복사
    │   │   scaled_width  = (384 * 4) / 3 = 512
    │   │   scaled_height = (799 * 4) / 3 = 1065
    │   └─ 결과: 54mm → 72mm (용지 너비 최적화)
    │
    ├─ [단계 4] ESC/POS 명령어 전송
    │   ├─ `1B 40`           → 프린터 초기화
    │   ├─ `1B 61 01`        → 가운데 정렬
    │   ├─ `1D 76 30 00 ...` → 래스터 비트맵 명령
    │   │   (w=512바이트, h=1065줄)
    │   ├─ [픽셀 데이터]     → 스케일된 비트맵 전송
    │   ├─ `1B 64 03`        → 3줄 피드
    │   ├─ `1D 56 41 00`     → 용지 부분 커팅
    │   └─ 각 명령어 사이 50~100ms 지연
    │
    └─ [단계 5] 메모리 해제
        free(scaled_pixel_data);
```

### PBM 헤더 파싱 예시
```
PBM 파일 내용 (바이너리):
╔════════════════════════════════════════╗
║ P4                  (P4 매직넘버)      ║
║ 384 799             (너비 높이)        ║
║ [아래 부터 픽셀 데이터]               ║
║ 48 바이트 × 799 줄 = 38352 바이트     ║
║ (384 픽셀 / 8 = 48 바이트)            ║
╚════════════════════════════════════════╝

파싱 과정:
1. "P4" 확인 ✓
2. 공백 스킵
3. "384" 파싱 → width = 384
4. 공백 스킵
5. "799" 파싱 → height = 799
6. 공백 스킵 → 다음 바이트부터 픽셀 데이터
```

### 스케일링 알고리즘 (nearest-neighbor)
```c
// 원본 384×799 → 스케일 512×1065

for (uint32_t y = 0; y < 799; y++) {
    for (uint32_t x = 0; x < 384; x++) {
        // 원본 픽셀 읽기
        uint8_t pixel = orig_pixel_data[...];
        
        // 스케일된 범위 계산
        uint32_t scaled_x_start = (x * 4) / 3;      // x=0: 0, x=1: 1, x=2: 2
        uint32_t scaled_x_end   = ((x + 1) * 4) / 3;
        uint32_t scaled_y_start = (y * 4) / 3;
        uint32_t scaled_y_end   = ((y + 1) * 4) / 3;
        
        // 해당 범위의 모든 스케일 픽셀에 같은 값 복사
        for (sy = scaled_y_start; sy < scaled_y_end; sy++) {
            for (sx = scaled_x_start; sx < scaled_x_end; sx++) {
                scaled_pixel_data[sy * scaled_bytes_per_row + sx/8] |= ...;
            }
        }
    }
}

// 메모리 사용:
// 스케일 버퍼 = (512 / 8) * 1065 = 64 * 1065 ≈ 68 KB ✓ 충분함
```

---

## 4️⃣ ESC/POS 명령어 완전 참고표

### 텍스트 인쇄 (`print_text()` 사용)
| 명령어 | HEX | 의미 | 설정값 |
|---|---|---|---|
| ESC @ | `1B 40` | 프린터 초기화 | 모든 설정 리셋 |
| ESC a | `1B 61 01` | 정렬 설정 | `01` = 가운데 정렬 |
| LF | `0A` | 줄바꿈 | 용지 1줄 이송 |
| GS V A | `1D 56 41 00` | 부분 커팅 | `00` = 일반 커팅 |

### 이미지 인쇄 (`parse_pbm_and_print()` 사용)
| 명령어 | HEX | 의미 | 파라미터 |
|---|---|---|---|
| GS v 0 | `1D 76 30 00` | 래스터 비트맵 인쇄 | 직후 xL xH yL yH |
| ESC d | `1B 64 03` | 피드 | `03` = 3줄 이송 |

**GS v 0 명령어 상세:**
```
명령어: 1D 76 30 00 [xL] [xH] [yL] [yH]
        ─────────┬───  ──────────┬────────
            고정         파라미터 (리틀엔디안 16비트)

xL, xH: 이미지 너비 (바이트 단위)
        width_bytes = (pixel_width + 7) / 8
        xL = width_bytes & 0xFF (하위 바이트)
        xH = (width_bytes >> 8) & 0xFF (상위 바이트)

yL, yH: 이미지 높이 (픽셀 줄 수)
        yL = height & 0xFF
        yH = (height >> 8) & 0xFF

예: 512×1065 스케일 이미지
    width_bytes = (512 + 7) / 8 = 65
    xL = 65 & 0xFF = 65 (0x41)
    xH = (65 >> 8) & 0xFF = 0
    yL = 1065 & 0xFF = 41 (0x29)
    yH = (1065 >> 8) & 0xFF = 4
    
    명령어: 1D 76 30 00 41 00 29 04
```

---

## 5️⃣ 기능 추가/수정 포인트

### 📌 케이스 1: 다른 이미지 출력하기

**목표**: `logo.pbm` 대신 다른 `custom.pbm` 이미지 출력

**수정 단계**:

1️⃣ `main/CMakeLists.txt` 수정
```cmake
# 기존:
EMBED_FILES "logo.pbm"

# 수정:
EMBED_FILES "logo.pbm" "custom.pbm"
```

2️⃣ `main/usb_printer_button_main.c` 수정
```c
// 추가 선언 (상단)
extern const uint8_t logo_pbm_start[] asm("_binary_logo_pbm_start");
extern const uint8_t logo_pbm_end[] asm("_binary_logo_pbm_end");

extern const uint8_t custom_pbm_start[] asm("_binary_custom_pbm_start");
extern const uint8_t custom_pbm_end[] asm("_binary_custom_pbm_end");

// parse_pbm_and_print() 함수 복제
static void parse_custom_pbm_and_print(void)
{
    // 동일한 로직, 단 pbm_data 초기화만 다름
    const uint8_t *pbm_data = custom_pbm_start;  // ◄ 변경
    size_t pbm_size = custom_pbm_end - custom_pbm_start;
    // ... 나머지 동일
}

// button_task에서 조건부 호출
if (stable_count >= debounce_threshold && curr_state == 0) {
    if (cdc_dev) {
        press_count++;
        if (press_count % 2 == 0) {
            parse_pbm_and_print();       // 짝수 번: logo.pbm
        } else {
            parse_custom_pbm_and_print();  // 홀수 번: custom.pbm
        }
    }
    ...
}
```

3️⃣ 빌드 및 플래싱
```bash
idf.py build
idf.py -p /dev/ttyACM0 flash
```

---

### 📌 케이스 2: 텍스트 추가 인쇄

**목표**: 버튼 누를 때 텍스트 + 이미지 모두 출력

**수정 위치**: `button_task` → `button_press #10` 코드
```c
if (stable_count >= debounce_threshold && curr_state == 0) {
    if (cdc_dev) {
        press_count++;
        
        // ★ 추가: 먼저 텍스트 인쇄
        char msg[64];
        snprintf(msg, sizeof(msg), "Press #%lu", press_count);
        print_text(msg);
        
        vTaskDelay(pdMS_TO_TICKS(500));  // 프린터 응답 대기
        
        // 이미지 인쇄 (커팅 없음 — 텍스트만 커팅됨)
        // parse_pbm_and_print() 에서 커팅 명령 제거 필요
        parse_pbm_and_print();
    }
    ...
}
```

**주의**: `print_text()`에는 이미 커팅 명령이 포함되어 있으므로,
추가로 `parse_pbm_and_print()`를 호출하면 텍스트-이미지 순서대로 출력됩니다.

---

### 📌 케이스 3: 버튼 여러 개 추가

**목표**: GPIO 4, 5에 추가 버튼 연결하여 다른 기능 실행

**수정 위치**: `button_task` 추가 또는 별도 태스크 생성
```c
#define BUTTON_PIN_1 GPIO_NUM_3
#define BUTTON_PIN_2 GPIO_NUM_4
#define BUTTON_PIN_3 GPIO_NUM_5

// 또는 별도 태스크
static void button_task_2(void *arg) {
    // GPIO 4 설정
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        // ...
    };
    
    while (1) {
        if (gpio_get_level(GPIO_NUM_4) == 0) {
            // GPIO 4 누름 시 다른 이미지 출력
            if (cdc_dev) {
                parse_custom_pbm_and_print();
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// app_main()에 추가
xTaskCreate(button_task_2, "button_task_2", 2048, NULL, 10, NULL);
```

---

### 📌 케이스 4: 인쇄 크기 변경

**목표**: 이미지를 더 크게 또는 작게 인쇄

**수정 위치**: `parse_pbm_and_print()` → 스케일 비율
```c
// 현재 (1.33배):
uint32_t scaled_width  = (width * 4) / 3;
uint32_t scaled_height = (height * 4) / 3;

// 2배로 확대:
uint32_t scaled_width  = width * 2;
uint32_t scaled_height = height * 2;

// 원본 크기 (1배):
uint32_t scaled_width  = width;
uint32_t scaled_height = height;

// 1.5배:
uint32_t scaled_width  = (width * 3) / 2;
uint32_t scaled_height = (height * 3) / 2;
```

⚠️ **주의**: 메모리 초과 주의!
- 원본 384×799 → 메모리 약 38KB
- 2배 768×1598 → 메모리 약 152KB (ESP32-P4는 충분하지만 위험)
- 1.33배 512×1065 → 메모리 약 68KB (안전)

---

## 6️⃣ 주요 API 참고 문서

| 항목 | 설명 | 공식 문서 |
|---|---|---|
| **USB Host** | USB 호스트 스택 초기화, 이벤트 처리 | [ESP-IDF USB Host](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/usb_host.html) |
| **CDC ACM** | 프린터 통신 (USB 가상 COM 포트) | [CDC ACM Host Component](https://components.espressif.com/components/espressif/usb_host_cdc_acm) |
| **GPIO** | 버튼 입력 처리 | [ESP-IDF GPIO](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/gpio.html) |
| **FreeRTOS** | 태스크 생성, 동기화 | [ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/system/freertos.html) |
| **ESC/POS** | 프린터 명령어 표준 | [Epson ESC/POS Manual](https://www.epson.co.jp/esc/pdf/ESC_POS_spec.pdf) |

---

## 7️⃣ 디버깅 팁

### 프린터가 연결되지 않음
```bash
# 시리얼 로그 확인
idf.py -p /dev/ttyACM0 monitor

# 예상 로그:
# I (1000) USB_PRINTER_BUTTON: ✓ Printer CONNECTED! VID=0x1504 PID=0x006e
```

### 이미지가 출력되지 않음
1. **프린터 연결 확인**: 위의 시리얼 로그에 "CONNECTED" 메시지 있는지 확인
2. **버튼 누름 확인**: 시리얼에 "Button Press #N - Printing image..." 메시지 있는지 확인
3. **PBM 파일 확인**: `logo.pbm` 파일이 valid P4 포맷인지 확인
   ```bash
   file logo.pbm
   # → "logo.pbm: PBM image data" 출력되어야 함
   ```

### 메모리 부족 (Heap overflow)
- `malloc()` 실패 → 스케일 비율 줄이기
- ESP-IDF 모니터링: `idf.py heap_tracing monitor`

---

## 📋 체크리스트: 코드 수정 전 확인사항

- [ ] 수정 전 `git status` 확인 (현재 상태 저장)
- [ ] `CLAUDE.md` 확인 (GPIO, 파티션 등)
- [ ] 새 이미지 파일 추가 시 `.pbm` 형식 확인
- [ ] `CMakeLists.txt` 문법 확인 (들여쓰기)
- [ ] 빌드: `idf.py build` (에러 확인)
- [ ] 플래싱: `idf.py -p /dev/ttyACM0 flash`
- [ ] 모니터링: 시리얼 로그 확인
- [ ] 수정 사항 커밋: `git commit -m "..."`

---

**마지막 업데이트**: 2026-04-24  
**문서 버전**: 1.0
