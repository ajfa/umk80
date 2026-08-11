# УМК-80 emulator

Emulator of the **УМК-80** (*Учебный микропроцессорный комплект*, "training
microprocessor kit"), a Soviet educational trainer built by the **ВЭФ**
association in Riga, Latvian SSR — part number `РР3.059.004` (briefcase) and
`РР3.059.004-01` (desktop). It is a teaching machine built around the
КР580ВМ80А, the Soviet clone of the Intel 8080.

It emulates the CPU cycle-accurately, the six multiplexed seven-segment
displays *with their real persistence*, the 24-key keyboard matrix, the three
rows of panel LEDs, and single-step execution both by instruction and by
machine cycle.

**The monitor ROM is included**, reconstructed from the assembler listing in
the original factory documentation. See [The ROM](#the-rom) below.

---

## Building

You need a C11 compiler and nothing else. The core does not even use the
standard library.

### Windows (primary target)

```
mingw32-make
```

The graphical frontend uses plain Win32 (`user32` + `gdi32`), which ships with
Windows itself: the `.exe` runs on a clean machine with no DLLs to copy.
Tested with gcc 16.2 from MSYS2 (`C:\msys64\mingw64\bin`).

### Linux and macOS

```bash
make
```

The graphical frontend uses SDL2 (`libsdl2-dev` on Debian and derivatives).
Everything else — core, tools, tests and headless mode — builds with no
dependencies at all.

Verified on Ubuntu 22.04 (WSL2, gcc 11, SDL2 2.0.20): builds without a single
warning, passes all four acceptance checks and the ROM cross-verification, and
the SDL2 window opens and responds to the keyboard. 8080EXM takes 34 s there.
macOS has not been tested.

### With CMake

An equivalent `CMakeLists.txt` is included if you prefer it:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

---

## Running

### Graphical panel

```
mingw32-make run
```

or directly `build/umk80 --rom rom/monitor.bin`.

Drive it by clicking the drawn keys, or from the host keyboard:

| Host | УМК-80 |
|---|---|
| `0`–`9`, `A`–`F` | hexadecimal keys |
| `F1` … `F6` | `П` `РГ` `СТ` `КС` `ЗК` `ПМ` |
| space | parameter separator |
| Enter | `ВП` (end of directive) |
| Esc | `СБ` (reset) |
| Backspace | `ПР` (interrupt) |
| `F8` | `ШГ` (step) |
| `F9` | `РБ/ШГ` — latches single-step mode |
| `F10` | `КМ/ЦК` — makes each step one machine cycle |

There are also two windowless modes, handy for tests and screenshots:

```
build/umk80 --rom rom/monitor.bin --keys "p0800.3E.AA.C3.00.08.>" --shot panel.ppm
```

### Debugger and headless mode

`umkcli` is both the headless mode and the debugger:

```bash
build/umkcli --rom rom/monitor.bin                  # interactive
build/umkcli --rom rom/monitor.bin -c "run 400000" -c display
```

Main commands (`help` lists them all):

| Command | What it does |
|---|---|
| `load <f> [addr]`, `loadhex <f>` | load raw binary or Intel HEX |
| `save`, `savehex` | dump memory |
| `run [cycles]`, `go <addr>` | run to a cycle budget or to a breakpoint |
| `step [n]`, `cycle [n]` | step by instruction / by machine cycle |
| `bp <addr>`, `bp list`, `bp del`, `bp clear` | breakpoints |
| `regs`, `reg <r> <v>`, `mem`, `poke`, `dis` | inspect and edit |
| `key <name>`, `keys a,b,c` | press panel keys |
| `display`, `panel` | seven-segment and LED state |
| `state save\|load <f>` | save and restore the whole machine |

### Standalone tools

```bash
build/umkasm program.asm program.bin        # 8080 assembler
build/umkdis program.bin --org 0x800        # disassembler
build/umkrom rom/monitor.lst rom/monitor.bin --asm rom/monitor.asm
```

---

## Checking that it works

```
mingw32-make test
```

runs the four acceptance checks:

1. **CPU validation** — TST8080, 8080PRE and CPUTEST. The fourth, 8080EXM,
   takes minutes and runs separately with `make test-exm`. All four pass, and
   8080EXM with all 26 CRCs correct.
2. **The real monitor** — types the full key sequence into the authentic
   monitor ROM and checks that `РГ` + `A` displays `A - AA`.
3. **Multiplexing fidelity** — a program that does *not* blank the displays
   before changing the segment mask must produce ghosting; the corrected one
   must show a clean `HELLO`.
4. **Single-step** — one press of `ШГ` advances exactly one instruction with
   `РБ/ШГ` latched, and exactly one machine cycle if `КМ/ЦК` is latched too.

Plus `make verify-rom`, explained next.

---

## The ROM

No public dump of the УМК-80 monitor ROM exists. This one is **reconstructed
from the assembler listing** included in the scanned factory documentation
(`Р.Р.00004-01 12 01-1`, «Системный монитор. Текст программы», 1986,
литера О<sub>I</sub>, printed by the ISIS-II 8080/8085 MACRO ASSEMBLER V4.0).

The transcription lives in [`rom/monitor.lst`](rom/monitor.lst), with the
object-code column and the source column side by side, and it is verified by
**two independent paths that must agree**:

```
make verify-rom
  ->  VERIFICATION OK: reassembled == OBJ column, 2048 bytes identical
```

- `umkrom` rebuilds the image from the listing's `OBJ` column;
- `umkasm` reassembles the source column.

For a transcription error to survive, it would have to be duplicated exactly
in both columns. On top of that, the resulting image boots and drives the
machine, which is the third check.

If you get hold of a real 1 KB ROM dump, it takes priority: load it with
`--rom` and the core will use it as is.

**Image contents** (2 KB, `0000h`–`07FFh`):

```
0000-03FF   Монитор            (the monitor)
0400-044B   ПРОГРАММАТОР УМК   (EPROM programmer, also in the listing)
```

---

## Machine map

```
0000-03FF   ПЗУ (ROM), monitor
0400-07FF   ПЗУ (ROM), second half
0800-0FFF   ОЗУ (RAM), 2 KB in the documented revision
   0FCE-0FF9   monitor variables and register table
   0FEE        interrupt vector table in RAM
   0FFA-0FFF   display refresh buffer (6 bytes)

F8   КР580ВВ55А port A — digit select and keyboard column scan
F9   КР580ВВ55А port B — segment mask
FA   КР580ВВ55А port C — keyboard rows (mask 74h)
FB   КР580ВВ55А control
FC   single-step mode (bit 0)
```

Two board profiles are selectable. The default is the documented revision
(2 KB of RAM); `--rev1` selects the earlier one, with 1 KB mirrored. The
reasoning is in [UNKNOWNS.md §1](UNKNOWNS.md) and [PLAN.md §1.1-bis](PLAN.md).

---

## Layout

```
core/        core: CPU, bus, ВВ55, displays, keyboard, panel, single-step
             — freestanding C11, no libc, no pointers in the state
frontend/    graphical panel: plain Win32 on Windows, SDL2 on POSIX
cli/         headless mode and debugger
tools/       assembler, disassembler, ROM reconstructor
rom/         listing transcription, source and image of the monitor
tests/       the four acceptance checks
docs/        provenance of the documentary sources
```

The core contains no monitor logic and no interface logic: the ROM is
pluggable and the panel is drawn outside it. `umk_machine_t` holds no
pointers, so saving and restoring the entire machine state is a struct copy.

---

## Documents

- **[PLAN.md](PLAN.md)** — the work plan, with what is documented and where
  every fact comes from.
- **[UNKNOWNS.md](UNKNOWNS.md)** — everything that could **not** be confirmed:
  what is known, why it is not enough, and how it was resolved or how it could
  be. Read it before trusting any fine detail.
- **[docs/SOURCES.md](docs/SOURCES.md)** — where each document comes from,
  with its SHA256.

## License and provenance

The emulator — core, frontend, debugger, tools and tests — is original work
and is released under the **[GNU General Public License v3.0](LICENSE)**.
Contributions require the agreement in [`CLA.md`](CLA.md); see
[`CONTRIBUTING.md`](CONTRIBUTING.md).

**The `rom/` directory is outside that license.** It holds the machine's
monitor, transcribed from the 1986 factory listing: Soviet documentation whose
corporate author no longer exists and over which the publisher of this
repository holds no rights, and therefore cannot sublicense. It is included for
preservation and interoperability — without the monitor the emulator does not
boot, and no public ROM dump exists — kept separate from the core and
replaceable by any other image via `--rom`. Details in
[`rom/PROVENANCE.md`](rom/PROVENANCE.md).

The panel is drawn from scratch using the manufacturer's own drawing (Рис. 2
of the ПС). The photographs in `docs/ref/`, used for the palette and the
proportions, belong to their authors and are **not** part of this repository
nor of what it distributes.
