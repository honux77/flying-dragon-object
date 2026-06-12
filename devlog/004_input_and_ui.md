# 004 — 입력 개선 및 환경설정 메뉴

**날짜**: 2026-06-11 ~ 2026-06-12  
**커밋**:
- `b3012fb` 입력 키 변경: Z=점프, X=공격, C=좌우 연타 터보
- `138d1e0` C 터보 전환 주기 2프레임으로 변경 (15Hz)
- `bdb7fab` 시작 전 환경설정 메뉴 추가 (키보드/조이스틱 설정)
- `f4cd1fb` 조이스틱 설정 개선: 방향키 버튼 매핑, 흔들기 버튼, Hat 지원

---

## b3012fb — 키 배치 변경 + 터보 구현

### 변경 이유

기존 키 배치가 직관적이지 않아 실제 플레이하기 불편했다.  
아케이드 레버+버튼 배치를 키보드에 맞게 재배치.

### 터보 (C키) 동작 원리

wbml에서 '달리기'는 별도 버튼이 없고, 같은 방향으로 빠르게 연타하면 달리기가 된다.  
C키를 누르는 동안 왼쪽/오른쪽 입력을 **매 프레임 교대**로 자동 입력한다.

```c
static unsigned turbo_tick = 0;
turbo_tick++;

if (k[cfg->k_turbo]) {
    if ((turbo_tick >> 1) & 1) p1 |= 0x80;  // 짝수 2프레임: 왼쪽
    else                        p1 |= 0x40;  // 홀수 2프레임: 오른쪽
}
```

`>> 1`은 2프레임 단위로 전환한다는 의미다. (15Hz = 60fps / 4)  
다음 커밋 `138d1e0`에서 전환 주기를 1프레임 → 2프레임으로 조정해  
더 자연스러운 달리기 속도를 얻었다.

---

## bdb7fab — 환경설정 메뉴

### 추가된 파일

| 파일 | 역할 |
|------|------|
| `cfg.c/h` | 키 바인딩 + 조이스틱 설정 구조체, 파일 저장/불러오기 |
| `menu.c/h` | SDL2 기반 메뉴 UI (게임 시작 전에 표시) |

### 설정 저장 위치

`SDL_GetPrefPath("wbml", "wbml")`를 사용해 OS별 사용자 디렉토리에 저장된다.

| OS | 경로 예시 |
|----|-----------|
| Windows | `C:\Users\<user>\AppData\Local\wbml\wbml\wbml.cfg` |
| macOS | `~/Library/Application Support/wbml/wbml/wbml.cfg` |
| Linux | `~/.local/share/wbml/wbml/wbml.cfg` |

SDL_GetPrefPath를 쓰면 OS마다 경로를 분기할 필요가 없다.

### 8×8 비트맵 폰트

메뉴 텍스트 렌더링을 위해 `font8x8_basic` 배열을 `menu.c`에 임베드했다.  
외부 폰트 파일 없이 실행 파일 하나로 동작하게 하기 위한 선택이다.  
각 문자는 8바이트, 각 바이트가 한 행의 8픽셀을 나타낸다.

---

## f4cd1fb — 조이스틱 개선

### 축(axis) → 개별 버튼 매핑으로 변경

기존: 아날로그 스틱의 X/Y축 값으로 방향 판정  
변경: 방향마다 독립적인 버튼 번호를 매핑

이유: XInput 컨트롤러의 D-패드(십자키)는 아날로그 축이 아닌  
**Hat 스위치** 또는 **디지털 버튼**으로 보고되기 때문에 축 기반이 동작하지 않았다.

### Hat 스위치 지원

SDL2에서 D-패드 Hat는 `SDL_JoystickGetHat()`으로 읽는다.  
에뮬레이터 내부에서는 음수 코드로 Hat 방향을 표현한다.

```c
// 인코딩: >= 0 = 버튼 번호,  -1 = 미할당
//          -2 = Hat 왼쪽, -3 = 오른쪽, -4 = 위, -5 = 아래
static int joy_dir(SDL_Joystick *joy, int v) {
    if (v == -1) return 0;
    if (v >= 0)  return SDL_JoystickGetButton(joy, v);
    Uint8 hat = SDL_JoystickGetHat(joy, 0);
    switch (v) {
    case -2: return (hat & SDL_HAT_LEFT)  != 0;
    // ...
    }
}
```

### 폰트 비트 순서 수정

`font8x8_basic`은 각 바이트의 **bit0이 왼쪽 픽셀** (LSB first) 규칙이다.  
기존 코드가 MSB first로 렌더링해서 글자가 좌우 반전되어 있었다.  
`(row >> (7 - col)) & 1` → `(row >> col) & 1` 로 수정.
