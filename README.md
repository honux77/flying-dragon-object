# Flydo — Wonder Boy: Monster Land 에뮬레이터

Sega System 2 기반 아케이드 게임 Wonder Boy: Monster Land (wbml)의 SDL2 에뮬레이터입니다.  
Windows / macOS 에서 동작합니다.

## 빌드

### macOS

```sh
brew install sdl2
./build.sh
```

### Windows (MinGW)

```sh
mingw32-make -C src/win-ref
```

### Windows (CMake + MinGW)

```sh
cmake -B cmake-build -S . -G "MinGW Makefiles"
cmake --build cmake-build
```

### Linux

```sh
sudo apt install libsdl2-dev cmake build-essential
./build.sh
```

## 실행

프로젝트 루트에서 실행해야 합니다. ROM 파일은 `roms/` 폴더에 있어야 합니다.

```sh
# macOS / Linux
./build/flydo

# Windows
build\flydo.exe

# ROM 경로를 직접 지정하는 경우
./build/flydo /path/to/roms
```

## ROM 파일 목록

`roms/` 폴더에 아래 파일들이 필요합니다.

| 파일 | 용도 |
|------|------|
| `epr-11031a.90` | 메인 CPU ROM 0 |
| `epr-11032.91`  | 메인 CPU ROM 1 |
| `epr-11033.92`  | 메인 CPU ROM 2 |
| `317-0043.key`  | MC-8123 복호화 키 |
| `epr-11037.126` | 사운드 CPU ROM |
| `epr-11034.4`   | 타일 ROM 0 |
| `epr-11035.5`   | 타일 ROM 1 |
| `epr-11036.6`   | 타일 ROM 2 |
| `epr-11028.87`  | 스프라이트 ROM 0 |
| `epr-11027.86`  | 스프라이트 ROM 1 |
| `epr-11030.89`  | 스프라이트 ROM 2 |
| `epr-11029.88`  | 스프라이트 ROM 3 |
| `pr11026.20`    | 컬러 PROM 0 |
| `pr11025.14`    | 컬러 PROM 1 |
| `pr11024.8`     | 컬러 PROM 2 |
| `pr5317.37`     | 룩업 PROM |

## 키보드 조작

### 게임 키 (환경설정 메뉴에서 변경 가능)

| 키 (기본값) | 동작 |
|-------------|------|
| 방향키 | 이동 |
| Z | 점프 |
| X | 공격 |
| C | 터보 좌우 이동 (터보 중 방향키 무시) |
| 5 | 코인 투입 |
| 1 | 1P 스타트 |

### 시스템 키

| 키 | 동작 |
|----|------|
| P | 일시정지 / 재개 |
| F5 | 리셋 |
| F6 | 현재 슬롯에 세이브 |
| F7 | 현재 슬롯에서 로드 |
| F8 | 이전 슬롯 선택 |
| F9 | 다음 슬롯 선택 |
| F12 | 스크린샷 저장 (`build/shot_N.bmp`) |
| ESC | 종료 |

세이브 슬롯은 5개이며 게임 내 RAM 전체 상태를 저장합니다.

### 환경설정 메뉴

게임 시작 전 환경설정 메뉴가 표시됩니다.

- **KEYBOARD SETTINGS** — 키보드 키 재매핑
- **JOYSTICK SETTINGS** — 조이스틱 버튼 매핑 (코인/스타트 포함)
- **DIFFICULTY** — EASY / NORMAL / HARD
- **CHEAT SETTINGS** — 치트 항목별 ON/OFF
  - FREEZE TIMER: 모래시계 타이머 정지
  - LEGEND ARMOUR / SHIELD: 방어구 최대 강화
  - SWORD OF LEGEND: 검 최대 강화
  - LEGEND BOOTS: 부츠 최대 강화
  - RICH GAME: 코인 획득 최소 보장
- **HELP** — 시스템 키 목록 표시

## 하드웨어 구성

| 항목 | 사양 |
|------|------|
| 메인 CPU | Z80 @ 4MHz (NEC MC-8123 암호화) |
| 사운드 CPU | Z80 @ 4MHz |
| 사운드 칩 | SN76489A × 2 |
| 해상도 | 256×224 (내부 512×224) |
| 프레임레이트 | 60.06 Hz |
