# PLAN — УМК-80 emulator

This document was written **before any code**, as the plan for the work, and
is kept as the record of what could be established from the documentation and
where each fact came from. Where it says "will", the thing has since been
built; the acceptance checks in `tests/` are the proof.

---

## 0. Summary

Three things turned out differently from the initial brief:

1. **The monitor ROM did not have to be reinvented or replaced.** The
   documentation PDF (`umk_docs.pdf`, 110 pages) contains, besides the
   operational manual, the document `Р.Р.00004-01 12 01-1` «Системный
   монитор — Текст программы» (1986, литера О<sub>I</sub>, 42 sheets): **the
   complete ISIS-II 8080/8085 MACRO ASSEMBLER V4.0 listing**, with the `LOC`
   (address) and `OBJ` (object bytes) columns printed. That makes it possible
   to rebuild the 1 KB image byte for byte *and* to verify it independently by
   reassembling the source text.

2. **The I/O ports are confirmed by the monitor's own source**, not by a blog
   post. And there is one more of them than expected.

3. **Multiplexing and the keyboard are the same loop**: the digit scan is also
   the keyboard column scan. That constrains the core design — the display
   cannot be modelled without modelling the keyboard at the same time.

4. **The memory map in the brief corresponds to an earlier revision of the
   machine.** The documentation carries revision ② (doc. `РР1323-87`, signed
   10.09.89), in which the "1 кбайт" of RAM is **struck through by hand and
   replaced with "2"**, and the line «в том числе, ПЗУ пользователя — 1
   кбайт» is **struck out entirely**. The parts list agrees: one RAM chip
   (`D24 КР537РУ8А`, 2 K×8) and one ROM chip (`D25 К573РФ2`, 2 K×8) — not two
   of each. And the monitor itself confirms it: `RAMEND EQU 1000H`. Detail in
   §1.1 and in UNKNOWNS.md §1.

---

## 1. What is confirmed from documentation

Everything in this section comes from the PDF (`docs/umk_docs.pdf`, SHA256 in
`docs/SOURCES.md`) unless stated otherwise. References are by PDF page.

### 1.1 Identity of the machine

| Item | Value | Source |
|---|---|---|
| Document | `РР3.059.004 ПС` — Паспорт, 38 sheets | p. 4 |
| Variants | `РР3.059.004` (briefcase, ≤455×421×130 mm, ≤9.6 kg) and `РР3.059.004-01` (desktop, ≤453×360×128 mm, ≤8.6 kg) | p. 9 |
| CPU | КР580ИК80А (= КР580ВМ80А) | p. 8 |
| RAM | ~~1~~ → **2 Кбайт** (handwritten correction, revision ②) | p. 8 |
| ROM | 2 Кбайт; the line «в том числе, ПЗУ пользователя — 1 кбайт» is **struck out** | p. 8 |
| Interrupt | 1 vector | p. 8 |
| Software | the «Монитор» program | p. 8 |
| I/O levels | TTL compatible, available on the ТЗЗ МI prototyping board | p. 8 |
| Power | 220 V ±22 V, 50±1 Hz; +5 V/0.70 A, +12 V/0.15 A, −5 V/0.20 A; ≤50 VA | p. 8 |
| Monitor | «занимает 1 Кбайт ПЗУ и использует последние 54 ячейки ОЗУ» | p. 16 |

### 1.1-bis The real memory map, and why it is not the one in the brief

Four independent sources converge:

| Source | What it says |
|---|---|
| ПС p. 8, revision ② (`РР1323-87`, 10.09.89) | RAM **2** Кбайт (the "1" struck by hand); ROM 2 Кбайт; the "user ROM" line struck out |
| ПЭ3 `РР3.055.472` sheet 2 (ТЗЗ ПЦМ) | **`D24 КР537РУ8А`** — 1 off (SRAM 2 K×8) · **`D25 К573РФ2`** — 1 off (EPROM 2 K×8, a 2716 equivalent) |
| ПС p. 25 (§4.5.1) | «оперативное запоминающее устройство (**D24**)»; «постоянное запоминающее устройство (**D25**)» — one chip each |
| The monitor source, p. 46 | `RAMEND EQU 1000H`, `BUFCD EQU 0FFAh`, `STKPTR EQU 0FCEh` |

Hence:

```
0x0000–0x03FF   Монитор            (К573РФ2, lower half)
0x0400–0x07FF   ПРОГРАММАТОР УМК   (К573РФ2, upper half)
0x0800–0x0FFF   RAM  2 KB          (КР537РУ8А)
   0x0FCE–0x0FF9  monitor variables + register table (LENTOS = 44)
   0x0FEE         interrupt vector table in RAM (USRSTTB)
   0x0FFA–0x0FFF  display refresh buffer (BUFCD, 6 bytes)
   monitor's initial SP = 0x0FCE
```

That is: **there is no reserved 1 KB of "user ROM"** — that half holds the
EPROM programmer — and **the RAM is 2 KB, not 1 KB**. The map in the brief
(and on eax.me, which is its origin) describes the earlier revision, with two
1 KB `К573РФ1` and two 512 B RAM chips.

Both are supported as **selectable machine profiles** (`--rev1` / `--rev2`,
defaulting to 2, the one the PDF documents), rather than picking one and
discarding the other. The core carries the decoding table as data, not as an
`#ifdef`.

### 1.2 Port map — confirmed from the monitor source

PDF page 46 (sheet «−4−», MODULE PAGE 3 of the listing), section
`1. ОПИСАНИЕ ПОРТОВ ВВОДА/ВЫВОДА`, transcribed verbatim:

```
00F8   PORTA    EQU  0F8H          ; ПОРТ АДРЕСА
00F9   PORTB    EQU  0F9H          ; ПОРТ ДАННЫХ
00FA   PORTC    EQU  0FAH          ; ПОРТ СОСТОЯНИЯ
00FB   CNTRRG   EQU  0FBH          ; ПОРТ УПРАВЛ. БИС
00FC   DBGPORT  EQU  0FCH          ; ПОРТ ПОШАГ. РЕЖ.
0076   CNTRWRD  EQU  76H           ; УПРАВЛЯЮЩЕЕ СЛОВО
0001   STEPWRD  EQU  1             ; УСТ. ПОШАГ. РЕЖИМА
0020   NMBIND   EQU  00100000B     ; N ИНДИКАТ. N 5
0352   TIME     EQU  850           ; ВРЕМЯ ДРЕБЕЗГА 10
0000   ERASE    EQU  0             ; СБРОС ИНДИКАЦИИ
0006   SPACE    EQU  6             ; КОД СИМВ.-ПРОБЕЛ
0007   CR       EQU  7             ; КОД СИМВ.-ВОЗВРАТ
0001   TYPEAD   EQU  1             ; ТИП ДАННЫХ-ADDRESS
0000   TYPEBT   EQU  0             ; ТИП ДАННЫХ - BYTE
1000   RAMEND   EQU  1000H         ; ВЕРХНЯЯ ГРАН. ОЗУ
002C   LENTOS   EQU  44            ; ДЛ.ТАБЛ.ИСХ.ЗНАЧ. РЕГ. И ВЕКТ.ПРЕРЫВ.
0FCE   BASETOS  EQU  RAMEND-LENTOS-6
0FCE   STKPTR   EQU  BASETOS       ; ИСХ.ЗНАЧ. SP МОНИТ.
0FFA   BUFCD    EQU  RAMEND-6      ; БУФЕР РЕГЕНЕР. ИНДИК.
0FEE   USRSTTB  EQU  BASETOS+32    ; АДР.ВЕКТ.ПРЕР. В ОЗУ
0000 C34000     JMP  BOOT
```

Direct consequences:

- **F8/F9/FA/FB are the four registers of a КР580ВВ55А (8255)**: A = digit
  select *and* keyboard column scan (output), B = segments (output),
  C = keyboard rows (input), FB = control register.
- The control word is written as `MVI A, NOT CNTRWRD` → `NOT 76H = 89H` (the
  listing prints `3E89` in the OBJ column, PDF p. 48). **0x89 = mode 0, PA
  output, PB output, PC input (both halves)** — exactly consistent with the
  described use. Source, object bytes and 8255 semantics all agree.
- **There is a fifth port the brief does not mention: `0FCH`, «ПОРТ ПОШАГ.
  РЕЖ.»**, with `STEPWRD = 1`. It is how the monitor arms single-step mode.

### 1.3 The display refresh loop — the heart of criterion 3

PDF page 72 (sheet «−30−», MODULE PAGE 29), verbatim:

```
02D1 0620          MVI  B,NMBIND    ; АДР.ИНДИК.
CILOOP:                             ; ЦИКЛ РЕГЕНЕРАЦИИ ; N ИНДИКАТОРА
02D3 78            MOV  A,B
02D4 D3F8          OUT  PORTA
                                    ; ВЫВОД ДАННЫХ НА ИНДИКАТОР
02D6 7E            MOV  A,M
02D7 D3F9          OUT  PORTB
                                    ; СКАНИРОВАНИЕ КОНСОЛИ
02D9 DBFA          IN   PORTC       ; ЧТЕНИЕ СОСТОЯНИЯ
02DB E674          ANI  74H         ; МОМЕНТ НАЖАТИЯ
02DD FE74          CPI  74H         ; КЛАВИШИ
                                    ; СБРОС ИНДИКАЦИИ
02DF 3E00          MVI  A,ERASE
02E1 D3F9          OUT  PORTB
02E3 C2F002        JNZ  CISMB       ; КЛАВИША НАЖАТА
02E6 23            INX  H           ; РЕГЕНЕРИРОВАТЬ СЛЕД.
02E7 78            MOV  A,B         ; ИНДИКАТОР
02E8 0F            RRC              ; N СЛЕД.ИНДИКАТОРА
02E9 47            MOV  B,A
02EA D2D302        JNC  CILOOP
02ED C3CE02        JMP  CIBEG       ; СНАЧАЛА
```

What this pins down:

- The scan starts at `B = 0x20` (bit 5) and **rotates right** down to `0x01`;
  rotating `0x01` sets the carry and restarts. Six positions, six digits.
- `HL` starts at `BUFCD = 0FFAh` and increments with the scan, so
  `0FFA ↔ bit5`, `0FFB ↔ bit4`, … `0FFF ↔ bit0`.
- **The monitor does blank the segments before advancing digit** (`MVI
  A,ERASE / OUT PORTB`). In other words, the monitor is itself the "correct"
  version of acceptance criterion 3, so a core that fails to model
  persistence would ghost *in the monitor too* — a free regression test.
- Keyboard mask `74H` = bits 2, 4, 5, 6 of PORTC → **4 rows × 6 columns = 24
  keys**, exactly the «24 клавиш, из них 8 клавиш директивные, а 16 —
  информационные» of the ПС (p. 34).
- Debounce is in software: `TIME EQU 850` («ВРЕМЯ ДРЕБЕЗГА 10» ms) and a
  `DELAY` routine at `035B`. **A delay loop calibrated in real cycles** —
  another reason cycle counting is not optional.

### 1.4 ROM structure

- `0000` → `JMP BOOT` (`C3 40 00`), BOOT at `0040`.
- RST 1..6 vectors at `0008`…`0030`, each `LHLD USRSTTB+2n / PCHL`
  (redirection through a table in RAM). RST 7 (`0038`) handles the **ПР**
  button interrupt.
- The monitor ends near `03E7` (`DS/DW` tables), so it fits in the 1 KB
  `0000–03FF`.
- **The second ROM half (`0400–07FF`) is not "reserved": it holds the
  «ПРОГРАММАТОР УМК»**, which the listing includes with `ORG 400H` (PDF pages
  82–83, code from `0400` to `0449`+). It is the К573РФ1 programmer, treated
  as an optional secondary ROM image, pluggable just like the first.

### 1.5 Monitor directives — confirmed formats

From the `Руководство оператора` (`Р.Р.00004-01 34 01-1`, 11 sheets, PDF pages
85–96) and the ПС (pp. 34–36):

| Directive | Format | Function |
|---|---|---|
| `П`  | `П XXXX ВП` | read/modify memory |
| `РГ` | `РГ Y ВП` | read/modify a register |
| `СТ` | `СТ [A1] [A2] [A3] ВП` | run; A2/A3 = up to **two** breakpoints |
| `КС` | `КС A1 A2 ВП` | checksum (modulo 256, no carry) |
| `ЗК` | `ЗК A1 A2 C ВП` | fill with a constant |
| `ПМ` | `ПМ A1 A2 A3 ВП` | copy a block |
| `_`  | (space) | parameter separator / advance one byte |
| `ВП` | | end of directive |

Registers addressable with `РГ`: `A B C D E H L P(признаков) SL SH PL PH`
(ПС p. 36 and the operator's manual p. 8). After `СБ` the display shows `-`;
on a syntax error it shows `?` and returns to the initial state.

Buttons and switches on the ПК board (ПС p. 28): momentary `СБ` (S1),
`ШГ` (S2), `ПР` (S5); **latching switches** `РБ/ШГ` (S3) and `КМ/ЦК` (S4).
So the latching behaviour is documented, and the official name is `КМ/ЦК`,
not `ММ/ЦИ`.

---

## 2. Language, dependencies and build

**Core: C11, freestanding.** Only `stdint.h`, `stddef.h`, `stdbool.h` —
headers the standard guarantees without a runtime library. No `stdio`, no
`stdlib`, no dynamic allocation: the entire machine state lives in a single
`umk_machine_t` owned by the caller. Rationale:

- It literally satisfies "no dependencies, not even the standard library",
  which in Rust would require `#![no_std]` plus hand-rolled `alloc` and would
  still drag in `cargo` and the network for any frontend crate, breaking
  "reproducible build with a single command" on a machine without access to
  crates.io.
- A POD `umk_machine_t` makes saving and restoring state a `memcpy` with a
  versioned header rather than a serialisation problem.

**Frontend: C11 with two platform backends behind a ~200-line layer.**

- **Windows (primary target): plain Win32** — `user32` + `gdi32`, which ship
  with the system. No external dependencies, no DLLs to copy; the portable
  zip runs on a double click.
- **POSIX: SDL2.** The lowest-friction route on Linux/macOS, and it does not
  contaminate the primary target.

The panel is rendered in software into a framebuffer owned by the frontend
(not by the emulator) and blitted with `StretchDIBits` or `SDL_UpdateTexture`,
so the drawing is identical on both platforms and testable without a window.

**Headless: C11 plus the standard library only.** No SDL, no Win32. This is
the binary the automated tests use.

**Build: a plain Makefile, one command per platform**, plus an equivalent
`CMakeLists.txt`. No downloads at build time. On Windows, MinGW-w64 (MSYS2)
is the reference toolchain and MSVC is supported; on Linux, gcc/clang with
`libsdl2-dev`.

---

## 3. Repository layout

```
core/                     freestanding, no libc
  include/umk80/          public API
  src/                    CPU, bus, ВВ55, display, keyboard, panel, step
tools/                    assembler, disassembler, ROM reconstructor
frontend/                 graphical panel
cli/                      headless mode and debugger
rom/                      listing transcription, source, image
tests/                    the four acceptance checks
docs/                     source provenance
```

---

## 4. The monitor ROM — procedure

Route 2 (reassemble from the listing), with double independent verification:

1. **Transcribe** the 39 listing pages (PDF pp. 45–83) into
   `rom/monitor.lst`, keeping the `LOC/OBJ` columns and the source column
   side by side.
2. **Rebuild** `monitor.bin` from `LOC/OBJ` (the `umkrom` tool). Gaps are
   `FF` and reported as gaps, never filled silently.
3. **Reassemble** `monitor.asm` with `umkasm`.
4. **Require byte-for-byte equality** between (2) and (3). Any discrepancy is
   a transcription or scanning error and is resolved by going back to the
   scan, never by bending the source to match the binary. Resolved
   discrepancies are recorded in the file.
5. **Functional verification**: the resulting ROM must pass acceptance
   criterion 2 in the emulator. If it does not, the transcription is not
   finished.

If a real 1 KB binary dump ever turns up it takes absolute priority and is
used to validate the transcription. The core contains **no** monitor logic in
any case.

---

## 5. Phases and exit criteria

CPU and its suites come first, before touching the panel.

| Phase | Content | Exit criterion |
|---|---|---|
| **F0** | Scaffolding: build, public header, minimal headless | builds on Windows and Linux |
| **F1** | Full 8080 CPU: 256 opcodes incl. undocumented, exact flags (AC included), PSW bit 1 set, exact cycle counts per instruction and per machine cycle | **TST8080, 8080PRE, CPUTEST and 8080EXM pass**, 8080EXM against the expected CRCs |
| **F2** | Bus, decoding/mirroring, КР580ВВ55А, port `0FCH`, 2 MHz timing | the monitor boots and shows `-` after `СБ` |
| **F3** | ROM transcription and verification | rebuilt `monitor.bin` == reassembled, byte for byte |
| **F4** | Headless frontend: load `.bin`/Intel HEX, run N cycles, dump registers/memory/display | criterion 2 passes headless |
| **F5** | Multiplexing persistence model | **criterion 3**: the naive program differs from HELLO, the corrected one matches |
| **F6** | Full panel and graphical frontend | the monitor is operable with the mouse |
| **F7** | Debugger: breakpoints, stepping by instruction and machine cycle, register and memory editing; disassembler | **criterion 4** |
| **F8** | Save/restore full state | save→restore→continue yields an identical trace |
| **F9** | Assembler + portable packaging | starts on a clean VM |

### The multiplexing model (F5)

Not six digits with independent state, but **a time integrator per (digit,
segment) pair**: 48 energy accumulators. As simulated time advances, each pair
(digit selected in PORTA × segment active in PORTB) accumulates the elapsed
interval, and all of them decay with a persistence constant of the order of
the real display's. The frontend draws the resulting intensity. With this:

- a program that does not blank before changing segments lights two patterns
  on the same digit during the same interval → ghosting, with no special-case
  code simulating it;
- the correct program produces a stable pattern;
- the automated test compares the **time-averaged state** over a window,
  independent of when the sample is taken.

The persistence constant is a parameter with a documented default, not a
magic number (see UNKNOWNS.md §5).

---

## 6. Risks

1. **Transcribing 39 pages of listing.** The longest task and the one most
   prone to silent error. Mitigated by the double verification of §4: a
   transcription error breaks the OBJ↔reassembly equality.
2. **The diagrams are scanned at low resolution** (~2200×1600 px for A1
   drawings). Reading the address decoder (`D15`, `D16` = К555ИД7) at track
   level may not be possible, and that is where mirroring comes from.
   Mitigated: mirroring does not affect whether the monitor works, only
   fidelity in unpopulated regions. See UNKNOWNS.md §1.
3. **8080EXM is slow.** It runs as a separate target, not on every commit.
