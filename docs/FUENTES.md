# Fuentes primarias

## `umk_docs.pdf`

- Origen: <https://xlat8086.com/downloads/umk/umk_docs.pdf>
- Descargado: 2026-08-11
- Tamaño: 26 002 784 bytes · 110 páginas · escaneado sin capa de texto
- SHA256: `0DFC93994A4FAAD99A3DFAEBC619A1AC6DA5C4E1FE3C18C76E2F57FA8CC7B18B`
- Metadatos: título «УМК Эксплуатационная документация», autor `maddev*xlat`,
  creado 2022-01-15.

No se versiona en el repositorio (26 MB). Reproducir con:

```bash
curl -o docs/umk_docs.pdf https://xlat8086.com/downloads/umk/umk_docs.pdf
```

y comprobar el SHA256 de arriba.

### Estructura por páginas del PDF

| Páginas | Documento |
|---|---|
| 1–3 | Cubierta y contenido del álbum |
| 4–38 | `РР3.059.004 ПС` — Паспорт (manual del equipo), 38 hojas |
| 39 | Лист регистрации изменений del ПС |
| 40–41 | Portada y anotación de `Р.Р.00004-01 12 01-1` — Системный монитор, **Текст программы**, 42 hojas, 1986, литера О₁ |
| 42–43 | Índice del listado |
| **44–83** | **Listado ISIS-II 8080/8085 MACRO ASSEMBLER V4.0 del monitor** (hojas −2− a −41−), con columnas `LOC`/`OBJ` |
| 84 | Лист регистрации изменений del listado |
| 85–96 | `Р.Р.00004-01 34 01-1` — Системный монитор, **Руководство оператора**, 11 hojas |
| 96 | `РР3.390.484 Э3` — Блок УМК, esquema eléctrico |
| 97 | `РР2.390.484 ПЭ3` — Блок УМК, perechen de elementos |
| 98–102 | `РР2.087.068` — Блок питания, esquema y perechen |
| 103 | `РР3.035.014 Э3` — ТЗЗ МI, esquema |
| 104 | `РР3.055.472 Э3` — ТЗЗ ПЦМ, esquema |
| 105–107 | `РР3.055.472 ПЭ3` — ТЗЗ ПЦМ, perechen de elementos (3 hojas) |
| 108–109 | `РР3.214.579 Э3` / `РР3.214.580 Э3` — Плата ПК y Плата ПИ, esquemas |
| 110 | Плата ПИ, perechen |

(La numeración de las páginas 96–110 se afinará al transcribir; las tres
últimas filas están tomadas de una vista de contactos, no de lectura directa.)

### Extracción de páginas a PNG

```bash
python tools/extract_pages.py docs/umk_docs.pdf docs/pages/
```

Las páginas nativas van a ~1019×1547 px (≈130 dpi) para el texto y a
~2200×1600 px para los esquemas de formato grande. El texto mecanografiado se
lee sin ambigüedad a resolución nativa; los esquemas, no siempre.

## Otras fuentes consultadas

| URL | Qué aporta | Fiabilidad |
|---|---|---|
| <https://eax.me/2023/2023-07-24-umk-80.html> | Puertos F8/F9, secuencias de teclado, programas de ejemplo, mapa de memoria | Buena para lo observable; **su mapa de memoria corresponde a una revisión anterior** — ver DESCONOCIDOS §1 |
| <https://www.computer-museum.ru/histussr/umk_sorucom_2011.htm> | Fabricante (ПО «ВЭФ», Riga), fechas, contraste con el УМПК-80 | Histórica; su afirmación de que el software es idéntico al del УМПК-80 **no se sostiene** — ver DESCONOCIDOS §2 |
| <https://github.com/GalaxyShad/UMPK-80-Emulator> | Emulador del **УМПК-80** (C++11/SFML/ImGui, GPL-3.0). Búfer de display en `0BFA–0BFF`, o sea distinto monitor | Referencia arquitectónica, no fuente de verdad del УМК |
| <https://habr.com/ru/companies/ruvds/articles/598697/> (+ 648649, 652151) | Artículos de dlinyj; fotos del panel | Pendiente de consultar; útil para la disposición física del teclado (DESCONOCIDOS §3, §4) |
