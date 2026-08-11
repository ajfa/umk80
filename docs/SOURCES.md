# Primary sources

## `umk_docs.pdf`

- Origin: <https://xlat8086.com/downloads/umk/umk_docs.pdf>
- Downloaded: 2026-08-11
- Size: 26,002,784 bytes · 110 pages · scanned, no text layer
- SHA256: `0DFC93994A4FAAD99A3DFAEBC619A1AC6DA5C4E1FE3C18C76E2F57FA8CC7B18B`
- Metadata: title «УМК Эксплуатационная документация», author `maddev*xlat`,
  created 2022-01-15.

Not versioned in this repository (26 MB). Reproduce it with:

```bash
curl -o docs/umk_docs.pdf https://xlat8086.com/downloads/umk/umk_docs.pdf
```

and check the SHA256 above.

### Page structure of the PDF

| Pages | Document |
|---|---|
| 1–3 | Cover and album table of contents |
| 4–38 | `РР3.059.004 ПС` — Паспорт (equipment manual), 38 sheets |
| 39 | Change record sheet for the ПС |
| 40–42 | Title page and abstract of `Р.Р.00004-01 12 01-1` — Системный монитор, **Текст программы**, 42 sheets, 1986, литера О<sub>I</sub> |
| **43–83** | **ISIS-II 8080/8085 MACRO ASSEMBLER V4.0 listing of the monitor** (sheets −1− to −41−), with `LOC`/`OBJ` columns |
| 84 | Change record sheet for the listing |
| 85–96 | `Р.Р.00004-01 34 01-1` — Системный монитор, **Руководство оператора** (operator's manual), 11 sheets |
| 96 | `РР3.390.484 Э3` — Блок УМК, circuit diagram |
| 97 | `РР2.390.484 ПЭ3` — Блок УМК, parts list |
| 98–102 | `РР2.087.068` — Блок питания (power supply), diagram and parts list |
| 103 | `РР3.035.014 Э3` — ТЗЗ МI, circuit diagram |
| 104 | `РР3.055.472 Э3` — ТЗЗ ПЦМ, circuit diagram |
| 105–107 | `РР3.055.472 ПЭ3` — ТЗЗ ПЦМ, parts list (3 sheets) |
| 108–109 | `РР3.214.579 Э3` / `РР3.214.580 Э3` — Плата ПК and Плата ПИ, diagrams |
| 110 | Плата ПИ, parts list |

(The numbering of pages 96–110 will be refined during transcription; the last
three rows come from a contact sheet, not from direct reading.)

Page 37 of the ПС carries the factory acceptance certificate of the specific
unit this documentation came with: `УМК РР3.059.004-01`, serial number
**18015**, date of manufacture **06.89**, handwritten and stamped by ОТК.

### Extracting pages to PNG

Pages render natively at ~1019×1547 px (≈130 dpi) for text and ~2200×1600 px
for the large-format diagrams. The typewritten text reads unambiguously at
native resolution; the diagrams do not always.

## Other sources consulted

| URL | What it contributes | Reliability |
|---|---|---|
| <https://eax.me/2023/2023-07-24-umk-80.html> | Ports F8/F9, key sequences, example programs, memory map | Good for observable behaviour; **its memory map corresponds to an earlier revision** — see UNKNOWNS §1 |
| <https://www.computer-museum.ru/histussr/umk_sorucom_2011.htm> | Manufacturer (ВЭФ association, Riga), dates, contrast with the УМПК-80 | Historical; its claim that the software is identical to the УМПК-80's **does not hold** — see UNKNOWNS §2 |
| <https://github.com/GalaxyShad/UMPK-80-Emulator> | Emulator of the **УМПК-80** (C++11/SFML/ImGui, GPL-3.0). Display buffer at `0BFA–0BFF`, i.e. a different monitor | Architectural reference, not a source of truth for the УМК |
| <https://habr.com/ru/companies/ruvds/articles/598697/> (+ 648649, 652151) | Articles by dlinyj; photographs of the panel | Used for panel colours and proportions |
