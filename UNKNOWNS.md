# UNKNOWNS — УМК-80

Everything that could **not** be confirmed from documentation, with what is
known, why that is not enough, and how it was resolved or how it could be.

Convention: **[A]** = blocks an acceptance criterion · **[B]** = affects
fidelity but does not block · **[C]** = cosmetic or documentary.

---

## 1. [B] Address decoding and mirroring

**What is known.** The ПС (p. 25) says the address decoder is `D15`, `D16`
(two К555ИД7, 3→8 decoders) plus `D5.1`, `D5.2` (halves of a К555ЛН2). There
are 2 KB of ROM at `0000–07FF` and 2 KB of RAM at `0800–0FFF`, i.e. two
contiguous 2 KB blocks. There is also a «схема блокировки памяти» (memory
lockout circuit: `D23.2`, `D14`, `D9.2`) tied to single-step mode.

**What is not known.** Which address lines feed the ИД7s. Two 3→8 decoders
give 16 outputs and only two are used. The most likely arrangement decodes
`A11–A13` (2 KB granularity, 16 KB of decoded space, mirrored every 16 KB
across the 64 K), but **that is an inference, not a fact**. Nor is it known
whether `0x1000–0xFFFF` answers with mirrors, with a floating bus, or with
`FF`.

**How to settle it.** Crop the `D15`/`D16` area out of diagram
`РР3.055.472 Э3` (page 104 of the PDF, 2230×1612 px) and read it enlarged. If
that is illegible, it stays a configurable parameter, defaulting to a 16 KB
mirror **and flagged as inferred**.

**Why it does not block.** Neither the monitor nor any of the acceptance-test
programs touch anything outside `0000–0FFF`.

---

## 2. [B] Is the УМК monitor the same as the УМПК-80's?

**What is known.** computer-museum.ru states that both «использовали
одинаковое системное ПО "Монитор"» (used identical "Monitor" system
software). GalaxyShad's emulator ships several binaries in `data/`
(`os.bin` 1712 B, `old.bin` 1712 B, `scaned-os-fixed.bin` 2048 B,
`scaned-os.bin` 3072 B) and its README places the seven-segment buffer at
`0BFA–0BFF`.

**What is not known.** Whether those binaries have anything to do with the
УМК. The УМК monitor's buffer is at **`0FFA–0FFF`**, not `0BFA–0BFF`. That
alone proves **they are not the same binary**, whatever the family
relationship. The museum's claim is at most "functionally equivalent".

**How it was handled.** Assume nothing: the УМК ROM is reconstructed from its
own listing (PLAN §4). GalaxyShad's binaries are used only as a contrast test
bench for the CPU core — never as a source of truth for the УМК.

---

## 3. ~~[A] Orientation of the six digits~~ — **RESOLVED 2026-08-11**

**Result: `OUT 0F8H` bit 0 is the leftmost display.**

**Starting point (apparent contradiction).** The refresh loop (PDF p. 72)
starts at `NMBIND = 0b00100000` (bit 5), rotates **right** down to bit 0, and
walks `BUFCD` **forwards** (`INX H`), so:

```
0FFA ↔ bit5   0FFB ↔ bit4   0FFC ↔ bit3   0FFD ↔ bit2   0FFE ↔ bit1   0FFF ↔ bit0
```

That by itself says nothing about which end of the panel is which.

**Decisive evidence** — routine `CO` (`0332`) and its shift loop `RALLP`
(`0344`), PDF pp. 75–76:

```
CO:  ; ... ЗАПИСЬ ПОЛУЧЕННОГО КОДА В БУФЕР ВЫВОДА. СДВИГ ТЕК. СОСТОЯНИЯ
     ;     ИНДИКАТОРОВ НА 1 ШАГ ВЛЕВО ...
     ; <C> — КОД СИМВОЛА ASCII
     ; <B> — ТИП ДАННЫХ, 0-BYTE, 1-ADDRESS

     ; СДВИГ ЕЛЕМЕНТОВ БУФЕРА ВЫВОДА НА 1 ШАГ ВЛЕВО В АДРЕСНОЙ ИЛИ
     ; БАЙТОВОЙ ЕГО ЧАСТИ. ЗП. В ПОЛЕ МЛ. ИНДИК-РА КОДА НОВОГО СИМВОЛА
0344 21FA0F   LXI  H,BUFCD
0347 1602     MVI  D,2
0349 78       MOV  A,B
034A B7       ORA  A
034B CA5203   JZ   RALLP
034E 23       INX  H          ; БУФЕР ДАННЫХ-ADDRES
034F 23       INX  H
0350 1604     MVI  D,4        ; 4 СДВИГА
RALLP:                        ; ЦИКЛ СДВИГА И ПЕРЕЗП. ЕЛЕМЕНТОВ БУФЕРА
0352 7E       MOV  A,M
0353 73       MOV  M,E
0354 5F       MOV  E,A
0355 23       INX  H
0356 15       DCR  D
0357 C25203   JNZ  RALLP
035A C9       RET
```

The new character is written into the **«мл. индикатор»** (lower index:
`BUFCD+0` for data, `BUFCD+2` for address) and the previous contents propagate
towards **higher indices** — and the monitor itself calls that «сдвиг на 1 шаг
влево», a shift one step to the *left*. Therefore **higher index = further
left**, and combined with the refresh-loop mapping: **`BUFCD+5` ↔ bit 0 = the
leftmost digit**.

**Independent corroboration 1** — `ERSBT`/`ERSADR` (`02B9`/`02C3`, PDF p. 71):

```
ERSBT:   ; ГАШЕНИЕ ИНДИКАЦИИ ДАННЫХ      (blank the data display)
02BC 22FA0F   SHLD BUFCD        ; blanks BUFCD+0, +1
ERSADR:  ; ГАШЕНИЕ АДРЕСНОЙ ИНДИКАЦИИ    (blank the address display)
02C6 22FC0F   SHLD BUFCD+2      ; blanks BUFCD+2, +3
02C9 22FE0F   SHLD BUFCD+4      ;    and BUFCD+4, +5
```

→ `BUFCD+0,+1` = ДАННЫЕ (data), `BUFCD+2..+5` = АДРЕС (address). With the
derived orientation, the panel reads left to right as:

```
  bit0   bit1   bit2   bit3  │  bit4   bit5
 BUF+5  BUF+4  BUF+3  BUF+2  │ BUF+1  BUF+0
 └────────── АДРЕС ─────────┘ └── ДАННЫЕ ──┘
      (MSD ............ LSD)   (MSD .. LSD)
```

which is exactly the `АДРЕС | ДАННЫЕ` layout of Рис. 2 of the ПС (p. 17), with
the hex digits in the correct reading order.

**Independent corroboration 2** — the monitor's start-up prompt (PDF p. 50,
sheet −8−) says it outright:

```
; ЕСЛИ МЛ. ИНДИКАТОР ПОГАШЕН, ВЫВЕСТИ "-" СЛЕВА, ИНАЧЕ ВЫВОД "-" СПРАВА
0063 0605     MVI  B,5      ; АДР.СТ.ИНДИКАТОРА   -> "-" on the LEFT
0068 0600     MVI  B,0      ; АДР.МЛ.ИНДИКАТОРА   -> "-" on the RIGHT
```

Index 5 = left, index 0 = right, stated in the source in so many words.

**Consequence for acceptance criterion 3.** The `HELLO` program starts with
`B = 0x01` and rotates left (`RLC`), reading `H,E,L,L,O` in ascending order:
`HELLO` comes out readable left to right. The orientation is still a core
parameter (`umk_display_set_digit_order`), defaulting to
`UMK_DIGIT_BIT0_LEFT`.

---

## 4. ~~[B] Exact 6×4 keyboard matrix map~~ — **RESOLVED 2026-08-11**

Derived entirely from routine `CONV` (`0302`–`0331`, listing sheets −31− to
−33−) once transcribed in full. No photograph was needed.

The row code that `CONV` computes from `PORTC` is 0, 4, 8 or 12 depending on
which bit is low (4, 6, 5 or 2 respectively); columns 0 and 1 of `PORTA` are
directive keys and columns 2 to 5 are digits:

```
digit    = (column - 2) + row_code            ; 0321  ADD C / ORI '0'
function = (column)     + row_code / 2        ; 032E  RRC  / ADD B
```

Hence the complete matrix:

|             | col bit0 | col bit1 | col bit2 | col bit3 | col bit4 | col bit5 |
|-------------|----------|----------|----------|----------|----------|----------|
| row bit 4   | `П`      | `РГ`     | `0`      | `1`      | `2`      | `3`      |
| row bit 6   | `СТ`     | `КС`     | `4`      | `5`      | `6`      | `7`      |
| row bit 5   | `ЗК`     | `ПМ`     | `8`      | `9`      | `A`      | `B`      |
| row bit 2   | `␣`      | `ВП`     | `C`      | `D`      | `E`      | `F`      |

24 keys: 8 directive + 16 informational, exactly the split given in the ПС
p. 34. The six directives yield codes 0 to 5, which is precisely the range
`START` accepts (`CPI 6 / JNC ERROR`) and the order of `CTBL` (`REPLM`,
`REPLRG`, `GOTO`, `CHSUM`, `FILE`, `MOVE`). Codes 6 and 7 are `SPACE EQU 6`
and `CR EQU 7`, i.e. the space key and `ВП`. Everything fits with no slack.

Independently corroborated by Рис. 2 of the ПС, where keys `4 5 6 7` carry
`PH PL SH SL` as a second legend and `8 9 A B` carry `H L A B` — exactly the
order of the monitor's `TBLRG` table.

**Verified in execution**: the acceptance-criterion-2 test types the whole
sequence against the real monitor and it works.

---

## 5. [B] Display persistence constant

**What is known.** The display has six multiplexed digits driven by `D20`,
`D21`, `D22` and transistors `VT1…VT6` (ПС p. 25). The monitor's refresh loop
spends a few hundred cycles per digit at 2 MHz.

**What is not known.** The exact indicator type, and therefore its real
persistence. The parts list for the ПИ board (PDF pages 108–110) should name
it (АЛС318, АЛС324 or similar), but it has not been read yet.

**How it is handled.** A decay constant that reproduces the described
observable behaviour (visible ghosting in the naive program, stability in the
corrected one), exposed as a documented parameter with a justified default,
not a magic number. **No automated test depends on the exact value**: the
criterion-3 comparison is over the time-averaged state, which is robust to it.

---

## 6. [B] Exact clock frequency

**What is known.** "≈ 2 MHz" (eax.me). The clock generator is a КР580ГФ24
(`D1`), which divides the crystal by 9.

**What is not known.** The crystal value. A ГФ24 with an 18.00 MHz crystal
gives exactly 2.000 MHz, which is typical; but the crystal is not in the parts
list read so far (it would be listed as `ZQ`), and has not been verified.

**Why it matters little.** Debounce (`TIME = 850`) and display brightness
depend on the frequency, but proportionally and without threshold effects. A
1 % error changes no observable behaviour.

**How it is handled.** Default 2.000 MHz, configurable.

---

## 7. [B] Semantics of port `0FCH` (ПОРТ ПОШАГ. РЕЖ.)

**What is known.** It exists, the monitor writes `STEPWRD = 1` to it, and the
ПС (p. 25) describes a «схема пошагового выполнения программ» (single-step
circuit) with ten logic elements (`D14`, `D19`, `D8.2`, `D23.4`, `D23.3`,
`D13.2`, `D5.4`, `D3.9`, `D10.1`, `D5.5`) plus a separate memory lockout
circuit. ПС p. 16 explains the mechanism: with `РБ/ШГ` in the `ШГ` position
the control unit enters a wait state after each step, and `КМ/ЦК` selects
whether a step is one instruction (the fetch of the first opcode byte) or one
machine cycle.

**What the full transcription added.** The monitor touches that port in
exactly two places, and both are unambiguous:

```
; RST 7 (the ПР button), listing sheet −6−
0039 AF     XRA  A                ; СБР.ПОШАГОВЫЙ РЕЖИМ  (clear step mode)
003A D3FC   OUT  DBGPORT

; immediately before handing control to the user program, sheet −38−
; (the EXIT routine, which executes from its copy in RAM)
03CF 3E01   MVI  A,STEPWRD        ; STEPWRD = 1
03D1 D3FC   OUT  DBGPORT
03D4 C34000 JMP  BOOT             ; PCLOC: the СТ directive rewrites the target
```

So: **bit 0 of port `0FCH` arms single-step mode when control passes to the
user program, and servicing `ПР` disarms it.** The `РБ/ШГ` switch therefore
only takes effect while user code is running — the monitor cannot single-step
itself.

**What is still unknown.** What bits 1 to 7 do, whether the port is readable,
and how the memory lockout circuit (`D23.2`, `D14`, `D9.2`) is wired into all
this.

**Modelling note.** With `КМ/ЦК` latched, the first press executes the
instruction "dry" to learn its machine-cycle sequence and then rolls
everything back (registers, RAM via a write journal, the ВВ55, the displays
and the panel); only pressing the **last** cycle executes it for real. So
machine state changes when it changes on the real machine, which is frozen by
READY between cycles. What is still not faithful: in an instruction with
several writes (`CALL`, `PUSH`) they all land together on the last cycle
instead of each on its own. That is not observable from the panel, which is
the only thing the machine shows in this mode.

---

## 8. [B] Polarity of the power LEDs

**The claim.** LED lit = that supply rail is **missing**.

**What is known.** The ПС describes an «устройство индикации аварии» (fault
indication device, p. 18) and a stabiliser lockout circuit; the three LEDs are
fault indicators, not presence indicators. eax.me states outright that the
LEDs light when the voltage is absent. Both are **consistent** with the claim,
but the literal phrasing «горит = нет напряжения» has not been read in the ПС
itself.

**How it is handled.** Implemented with that polarity, which is what makes
sense for a fault indicator, and noted here until the exact sentence in ПС
§4.3 (p. 18) or the power supply diagram (PDF page 98) can be quoted.

---

## 9. [B] Undocumented КР580ВМ80А instructions

**What is known.** The eight undocumented 8080 opcodes (`08`, `10`, `18`,
`20`, `28`, `30`, `38` = `NOP`; `CB` = `JMP`; `D9` = `RET`; `DD`/`ED`/`FD` =
`CALL`) are well characterised for the Intel 8080, and 8080EXM exercises them.

**What is not known.** Whether the Soviet clone behaves identically in all of
them. No source documenting a divergence has been found, but "not found" is
not "does not exist".

**How it is handled.** Intel 8080 semantics are implemented (which is what the
acceptance suites require) and the question is recorded here. If a divergence
is ever documented, it becomes another machine-profile parameter.

---

## 10. [C] Switch nomenclature

Some sources call the cycle-step switch `ММ/ЦИ`. The official documentation
(ПС p. 28, list of parts on the ПК board) says **`КМ/ЦК`** (`S4`), alongside
`РБ/ШГ` (`S3`), `СБ` (`S1`), `ШГ` (`S2`) and `ПР` (`S5`). `КМ/ЦК` is used as
the canonical name, with `ММ/ЦИ` accepted as an alias in key configuration.

---

## 11. [C] What the ROM socket actually holds in real units

The listing includes a «ПРОГРАММАТОР УМК» (EPROM programmer) at `ORG 400H`.
Revision ② of the ПС struck out the "user ROM" line, which suggests that in
the final revision the second kilobyte holds the factory programmer. But
whether **every** unit shipped with it burned in is unknown. It is treated as
an optional image: if not loaded, that half reads `FF`.

---

## 12. [C] Memory chips per physical unit

Three different inventories exist for the same machine:

| Source | RAM | ROM |
|---|---|---|
| This documentation, ПЭ3 `РР3.055.472` sheet 2 | `D24 КР537РУ8А`, one unit, 2 K×8 | `D25 К573РФ2`, one unit, 2 K×8 |
| eax.me | 2 × К537РУ13 | 2 × К573РФ1 |
| Reported elsewhere | 2 × КР541РУ2 | — |

This confirms there were at least two board revisions. It does not affect
emulation beyond the sizes, which the two machine profiles already cover
(PLAN §1.1-bis), but it is worth knowing that **none of the three is
necessarily wrong**.

---

## 13. [C] Dates and variants

computer-museum.ru dates the УМК to 1983–1985. The documentation held here is
signed 1987–1989 and the monitor listing carries `1986, литера О₁`. The
acceptance certificate of the specific unit it came with is dated 06.89. One
article dates the machine to 1988. All of these are probably true for
different revisions; the point is recorded so that no single date gets
repeated as if it were the only one.
