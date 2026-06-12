# 005 — 크로스 플랫폼 빌드 + 프로젝트 목표 변경

**날짜**: 2026-06-12  
**커밋**: `3e69102` 디버깅 중 임시 커밋, `956d9fb` Add roms folder  
**현재 세션 변경**: CMakeLists.txt 추가, romdir 경로 수정, 실행 파일명 flydo로 변경

---

## 프로젝트 목표 변경

원래 목표: Windows 레퍼런스 에뮬레이터 → MSX2+ 포팅  
**변경 후 목표**: Windows / macOS 양 플랫폼에서 완성도 높게 동작하는 에뮬레이터

MSX2+는 추후 과제로.

---

## 빌드 시스템 변경

### 기존

`src/win-ref/Makefile` 하나. MinGW 전용, `mingw32-make`로만 빌드 가능.

```makefile
LIBS := -lmingw32 -lSDL2main -lSDL2
$(EXE): ...
    $(CC) ... -mwindows   # Windows PE 전용 플래그
```

### 변경 후

**`CMakeLists.txt`** (프로젝트 루트)를 추가해 세 플랫폼을 지원.

| 플랫폼 | SDL2 탐색 방법 |
|--------|---------------|
| Windows | `third_party/SDL2` (벤더 포함) → CMake 프리픽스 경로 |
| macOS | `/opt/homebrew` (Apple Silicon) 또는 `/usr/local` (Intel Homebrew) |
| Linux | 시스템 패키지 (`apt install libsdl2-dev`) |

**핵심**: C 소스는 이미 SDL2 크로스 플랫폼 API만 사용하고 있었으므로  
빌드 설정 파일만 추가하면 코드 수정 없이 맥에서 빌드된다.

### macOS에서 rpath 설정

```cmake
set_target_properties(flydo PROPERTIES
    BUILD_RPATH  "/opt/homebrew/lib;/usr/local/lib"
    INSTALL_RPATH "/opt/homebrew/lib;/usr/local/lib"
)
```

rpath란 실행 파일이 동적 라이브러리를 찾는 경로를 실행 파일 내부에 기록하는 것이다.  
이것 없으면 `./flydo` 실행 시 SDL2.dylib를 찾지 못해 "Library not loaded" 오류가 난다.  
(macOS는 Linux의 `LD_LIBRARY_PATH` 대신 rpath를 선호한다.)

---

## ROM 경로 단순화

```c
// 수정 전
const char *romdir = (argc > 1) ? argv[1] : "roms/wbml";

// 수정 후
const char *romdir = (argc > 1) ? argv[1] : "roms";
```

실제 롬 파일이 `roms/wbml/` 하위 폴더에 있었으나  
불필요한 중첩 폴더를 제거하고 `roms/`에 직접 배치.

에러 메시지도 개선:
```c
// 수정 전
fprintf(stderr, "machine_init failed (check ROM path: %s)\n", romdir);

// 수정 후: machine.c에서 개별 파일명을 이미 출력하므로 안내 메시지만
fprintf(stderr, "Place the ROM files listed above in '%s/' and retry.\n", romdir);
```

---

## 실행 파일명 변경

`wbml-ref` → **`flydo`**  
(프로젝트 저장소 이름 `flying-dragon-object`에서)

---

## 검증

```
$ cmake -B cmake-build -S . && cmake --build cmake-build
[100%] Linking C executable .../build/flydo
[100%] Built target flydo

$ file build/flydo
build/flydo: Mach-O 64-bit executable arm64

$ otool -L build/flydo
/opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib ...
```

macOS Apple Silicon(arm64)에서 빌드 및 SDL2 링크 확인.
