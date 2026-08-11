# Provenance of the contents of `rom/`

**This directory is not original work, and the repository's MIT licence does
not cover it.**

The `LICENSE` at the root covers the emulator: the core, the frontend, the
debugger, the tools and the tests. What lives here is something else.

## What is here

| File | What it is |
|---|---|
| `monitor.lst` | Transcription of the monitor's assembler listing, with the object-code column and the source column side by side |
| `monitor.asm` | The source column, extracted from `monitor.lst` |
| `monitor.bin` | The 2 KB image, rebuilt from the object-code column |

## Where it comes from

From document **`Р.Р.00004-01 12 01-1`**, «Учебный микропроцессорный
комплект. Системный монитор. Текст программы» ("Training microprocessor kit.
System monitor. Program text"), 42 sheets, **1986**, литера О<sub>I</sub>,
printed by the ISIS-II 8080/8085 MACRO ASSEMBLER V4.0. It is part of the
factory documentation album for the УМК-80, manufactured by the **ВЭФ**
association of Riga, Latvian SSR.

The scan it was transcribed from is recorded in `docs/SOURCES.md`, with its
SHA256.

## Status

The program dates from 1986 and its corporate author, the ВЭФ association,
ceased to exist along with the Soviet Union. There is no rights holder to ask
for permission or to negotiate terms with, and whoever publishes this **holds
no rights over that work**: they therefore cannot license it to third parties
under MIT or anything else.

It is included for **historical preservation and interoperability**: without
the monitor, an УМК-80 emulator does not boot, and no public dump of the ROM
exists. It lives in this directory, separate from the emulator core, and it is
replaceable — the emulator accepts any other image via `--rom`.

If you are, or represent, a rights holder in this material and want it taken
down, please open an issue on the repository.
