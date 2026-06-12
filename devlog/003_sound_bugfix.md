# 003 — 사운드 버그 수정 (상세 분석)

**날짜**: 2026-06-11  
**커밋**: `cbf943b` 효과음 버그 수정: 사운드 RAM 주소 미러링 및 Z80 NMI/PPI 핸들링 수정  
**수정 파일**: `machine.c`, `z80/z80.c`, `dbg.c`

---

## 증상 요약

- 부팅 음과 코인 SFX가 MAME 대비 약 **52프레임(0.87초) 지연** 후 재생됨
- MAME와 로컬 에뮬레이터의 SN76489 레지스터 기록을 비교했을 때:
  - MAME: 커맨드 0x17 수신 → 즉시 `82 0B 90 A2 0B B0` 쓰기
  - 로컬: 커맨드 0x17 수신 → `80 00 80 A0 00 A0` 쓰기 → **52프레임 후** `82 0B 90 A2 0B B0` 쓰기

---

## 디버깅 방법

1. **MAME Lua 스크립트**: MAME의 Lua API로 메모리 탭(tap)을 걸어  
   커맨드 쓰기(`OUT 0x14`)와 SN76489 레지스터 쓰기를 프레임 번호와 함께 기록
2. **로컬 SN 로그**: `SN_LOG=1` 환경변수 또는 `--snlog` 플래그로 동일 포맷 기록
3. **사운드 RAM 덤프**: `dbg.c`에서 주요 RAM 주소값을 프레임 단위로 출력
4. **디스어셈블리**: `tools/disasm.py`로 사운드 ROM을 역어셈블해  
   NMI 핸들러와 메인 루프 구조를 파악

---

## 발견한 버그 3개

### 버그 1: 사운드 SRAM 주소 미러링 누락 (핵심 원인)

#### 하드웨어 사실

Sega System 2의 사운드 CPU 메모리맵에는 **2KB SRAM 칩 1개**가 있다.  
이 칩은 주소 버스의 **A10:A0 (11비트)** 만 디코딩하고 A11 이상은 무시한다.  
따라서 `0x8000–0x9FFF` (8KB 범위) 어디를 읽어도 같은 2KB 셀에 접근된다.

```
주소 버스 (16비트)
│  A15..A12 │ A11  │ A10..A0 │
│   (CS 선택)│(무시) │  (SRAM 내부 주소) │

0x8072 → A11=0, A10:A0 = 0x072  → SRAM[0x072]
0x8872 → A11=1, A10:A0 = 0x072  → SRAM[0x072]  ← 동일한 셀!
0x887A → A11=1, A10:A0 = 0x07A  → SRAM[0x07A]  ← SRAM[0x07A]와 동일
```

#### 버그 코드

```c
// 수정 전: 0x8000-0x87FF 범위(2KB)만 처리하고 나머지는 무시
if (addr >= 0x8000 && addr < 0x8800) return m->soundram[addr - 0x8000];
```

#### 문제가 되는 사운드 ROM 동작

사운드 드라이버 내부에서 `RST 18h` 명령이 실행될 때  
**D 레지스터가 의도치 않게 오염**되어 `0x8872` / `0x887A`에 쓰기가 발생한다.  

실제 하드웨어에서는 미러링 때문에 이 쓰기가 `0x8072` / `0x807A`에 도달하는데,  
에뮬레이터에서는 `0x8800` 이상을 무시하므로 **해당 쓰기가 그냥 버려진다**.

#### `0x8072`가 무슨 값인가

사운드 드라이버의 **그룹 카운터(tempo timer)** 변수다.  
커맨드 처리 전에 `0xD1`(209) 같은 값으로 초기화되어야 하는데,  
쓰기가 무시되니 이전 값이 그대로 남아 있다.

드라이버 루틴 `0x021E`는 매 IRQ마다 이 카운터를 1씩 감산하고  
카운터가 0이 될 때까지 다음 이벤트를 읽지 않는다.  
초기화가 누락된 채로 기존 값에서 카운트다운하니 **52프레임 지연**이 발생한다.

#### 수정 코드

```c
// 수정 후: A10:A0만 인덱스로 사용 → 0x8000-0x9FFF 전체를 동일 2KB에 미러링
if (addr >= 0x8000 && addr < 0xA000) return m->soundram[addr & 0x7FF];
```

---

### 버그 2: Z80 NMI 처리 시 IFF1/IFF2 저장 누락

#### Z80 아키텍처 배경

Z80에는 인터럽트 플래그가 두 개 있다.

| 플래그 | 역할 |
|--------|------|
| `IFF1` | 마스커블 인터럽트(INT) 허용 여부를 직접 제어 |
| `IFF2` | IFF1의 백업. RETN 명령이 IFF2 → IFF1으로 복원 |

NMI(Non-Maskable Interrupt)는 이름처럼 **마스킹 불가**다.  
IFF1이 0이어도 (DI 상태여도) 반드시 처리된다.

**실제 Z80의 NMI 처리 순서:**
1. `IFF2 ← IFF1` (현재 인터럽트 허용 상태를 백업)
2. `IFF1 ← 0` (NMI 처리 중 추가 INT 차단)
3. PC를 스택에 push
4. 0x0066으로 점프 (NMI 벡터)

**RETN 명령:**
1. 스택에서 PC 복원
2. `IFF1 ← IFF2` (백업해 둔 상태로 복원)

#### 버그 코드

```c
// 수정 전: IFF2에 저장 없이 IFF1만 클리어
if (z->nmi_pending) {
    z->nmi_pending = 0;
    z->halted = 0;
    z->iff1 = 0;   // IFF2 저장 없이 클리어!
    // ...push PC, jump to 0x0066
}
```

#### 영향

사운드 CPU의 NMI 핸들러가 `RETN`으로 끝나는데,  
`IFF2`에 올바른 값이 없으면 `RETN` 이후 `IFF1`이 잘못된 상태가 된다.  
사운드 드라이버가 EI/DI로 인터럽트를 관리하는 부분에서  
타이밍이 틀려져 IRQ 처리 횟수가 달라진다.

#### 수정 코드

```c
// 수정 후: IFF1 → IFF2 백업 후 클리어
if (z->nmi_pending) {
    z->nmi_pending = 0;
    z->halted = 0;
    z->iff2 = z->iff1;   // 백업
    z->iff1 = 0;
    // ...
}
```

---

### 버그 3: NMI 체크와 EI 딜레이 처리 순서

#### Z80 명세

NMI는 **비마스커블**이므로 EI 명령 직후의 1명령 딜레이와 무관하게 즉시 처리되어야 한다.  
즉, `EI` → `NMI` 순서라면 딜레이 중이어도 NMI는 실행된다.

#### 버그 코드

```c
// 수정 전: EI 딜레이를 먼저 처리하고 나서 NMI를 체크
void process_interrupts(z80 *z) {
    if (z->iff_delay > 0) { /* EI 딜레이 처리 */ return; }
    if (z->nmi_pending) { /* NMI 처리 */ }
    if (z->int_pending && z->iff1) { /* INT 처리 */ }
}
```

EI 딜레이 중에 NMI가 pending되어 있어도 그 프레임에서 처리되지 않는다.

#### 수정 코드

```c
// 수정 후: NMI를 가장 먼저 체크 (EI 딜레이보다 우선)
void process_interrupts(z80 *z) {
    if (z->nmi_pending) { /* NMI 처리 */ return; }
    if (z->iff_delay > 0) { /* EI 딜레이 처리 */ return; }
    if (z->int_pending && z->iff1) { /* INT 처리 */ }
}
```

---

### 추가: PPI 8255 Mode 2 핸들링 개선

#### 배경

8255 PPI는 포트 A를 **Mode 2 (양방향 버스)**로 사용한다.  
Mode 2에서 포트 A에 데이터를 쓰면 **OBF (Output Buffer Full)** 라인이 Low로 떨어지고,  
상대방(사운드 CPU)이 데이터를 읽으면 다시 High로 올라간다.

wbml에서는 OBF가 사운드 CPU의 NMI 핀에 연결되어 있다.  
따라서 "데이터를 썼다" → "OBF Low" → "NMI 발생" 순서다.

#### 수정 전 구현

```c
// OUT 0x14 (포트 A 쓰기) → 즉시 NMI 발생
case 0x14:
    m->soundlatch = val;
    z80_gen_nmi(&m->soundcpu);   // 바로 NMI
    break;
```

#### 수정 후 구현

OBF 라인(PPI portC bit7)의 **하강 에지**를 검출해서 NMI를 발생시킨다.

```c
// sound_control_w: portC bit7이 1→0으로 바뀔 때 NMI 발생
void sound_control_w(system2 *m, uint8_t data) {
    int prev_clear = m->sound_nmi_mask;  // 이전에 bit7이 1이었는가
    m->ppi_portc = data;
    m->sound_nmi_mask = (data & 0x80) ? 1 : 0;
    if (prev_clear && !m->sound_nmi_mask) {  // 1→0 하강 에지
        z80_gen_nmi(&m->soundcpu);
    }
}

// OUT 0x14 → portC bit7/6을 0으로 내림 → sound_control_w가 NMI 발생
case 0x14:
    m->soundlatch = val;
    sound_control_w(m, (uint8_t)(m->ppi_portc & ~0xc0));
    break;
```

---

## 수정 결과

| 항목 | 수정 전 | 수정 후 |
|------|---------|---------|
| 부팅 음 지연 | ~52프레임 | 없음 |
| 코인 SFX 길이 | 길게 늘어짐 | 정상 |
| SN2 첫 쓰기 타이밍 | cmd 후 52프레임 | cmd 직후 |

---

## 공부하고 싶다면

- **SRAM 미러링**: 데이터시트에서 주소 디코더 회로를 보면 어떤 비트가 칩 선택에 사용되는지 알 수 있다. MAME 소스의 `ADDRESS_MAP` 정의에서 미러링은 `mirror(0x...)` 매크로로 표현된다.
- **Z80 IFF/NMI**: "Z80 CPU User Manual" (Zilog 공식 문서) 의 인터럽트 섹션. 특히 표 "Interrupt Response Cycles"와 RETN/RETI 설명을 보면 IFF1/IFF2의 역할이 명확하다.
- **PPI 8255 Mode 2**: Intel 8255A 데이터시트의 Mode 2 타이밍 다이어그램. OBF#, ACK#, STB#, IBF 신호의 하드웨어 악수(handshake) 과정.
- **MAME 비교 디버깅**: `build/mame_soundtap.lua` 스크립트처럼 Lua로 메모리/포트 탭을 걸면 사이클 단위 비교가 가능하다. MAME의 `-debugscript` 옵션도 유용하다.
