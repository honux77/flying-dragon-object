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

| 키 | 동작 |
|----|------|
| Z | 점프 |
| X | 공격 |
| C | 터보 좌우 이동 |
| 방향키 | 이동 |
| 5 | 코인 투입 |
| 1 | 1P 스타트 |
| F12 | 스크린샷 저장 (`build/frame_N.ppm`) |
| ESC | 종료 |

조이스틱 및 키보드 설정은 시작 시 나타나는 환경설정 메뉴에서 변경할 수 있습니다.

## 하드웨어 구성

| 항목 | 사양 |
|------|------|
| 메인 CPU | Z80 @ 4MHz (NEC MC-8123 암호화) |
| 사운드 CPU | Z80 @ 4MHz |
| 사운드 칩 | SN76489A × 2 |
| 해상도 | 256×224 (내부 512×224) |
| 프레임레이트 | 60.06 Hz |
