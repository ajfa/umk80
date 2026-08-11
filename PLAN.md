# PLAN — Emulador del УМК-80 (ВЭФ, РР3.059.004)

Estado: **propuesta, pendiente de tu visto bueno.** No se ha escrito nada de código.
Fecha: 2026-08-11.

---

## 0. Resumen ejecutivo

Tres cosas cambian respecto a lo que suponía tu encargo:

1. **La ROM del monitor no hay que reinventarla ni sustituirla.** El PDF
   `umk_docs.pdf` (110 páginas) incluye, además del paquete de documentación
   operativa, el documento `Р.Р.00004-01 12 01-1 "Системный монитор — Текст
   программы"` (1986, литера О₁, 42 hojas): **el listado completo del
   ensamblador ISIS-II 8080/8085 MACRO ASSEMBLER V4.0**, con las columnas
   `LOC` (dirección) y `OBJ` (bytes objeto) impresas. Eso permite reconstruir
   la imagen de 1 KB byte a byte *y* verificarla de forma independiente
   reensamblando el texto fuente. La ruta 3 (monitor sustituto) queda como
   plan de contingencia, no como camino previsto.

2. **Los puertos de E/S están confirmados por el propio fuente del monitor**,
   no por un artículo de blog. Y hay uno más de los que menciona tu encargo.

3. **El multiplexado y el teclado son el mismo lazo**: el barrido de dígitos
   es también el barrido de columnas del teclado. Eso condiciona el diseño del
   núcleo (no se puede modelar el display sin modelar el teclado a la vez).

4. **El mapa de memoria de tu encargo corresponde a una revisión anterior del
   equipo.** La documentación que descargué lleva la revisión ② (doc.
   `РР1323-87`, firmada 10.09.89) y en ella el «1 кбайт» de ОЗУ está **tachado
   a mano y sustituido por «2»**, y la línea «в том числе, ПЗУ пользователя —
   1 кбайт» está **tachada entera**. El perechen de elementos concuerda: un
   solo chip de ОЗУ (`D24 КР537РУ8А`, 2 K×8) y un solo chip de ПЗУ
   (`D25 К573РФ2`, 2 K×8) — no dos de cada uno. Y el propio monitor lo
   confirma: `RAMEND EQU 1000H`. Detalle completo en §1.1 y en
   DESCONOCIDOS.md §1.

---

## 1. Lo que está verificado documentalmente

Todo lo de esta sección procede del PDF descargado hoy
(`docs/umk_docs.pdf`, SHA a registrar en el repo) salvo donde se indique.
Referencio por página del PDF y, entre paréntesis, por página impresa del
documento correspondiente.

### 1.1 Identidad del equipo

| Dato | Valor | Fuente |
|---|---|---|
| Documento | `РР3.059.004 ПС` — Паспорт, 38 hojas | p. 4 |
| Variantes | `РР3.059.004` (maletín, ≤455×421×130 mm, ≤9,6 kg) y `РР3.059.004-01` (sobremesa, ≤453×360×128 mm, ≤8,6 kg) | p. 9 |
| CPU | КР580ИК80А (= КР580ВМ80А) | p. 8 (tabla «Технические характеристики») |
| ОЗУ | ~~1~~ → **2 Кбайт** (corrección manuscrita, revisión ②) | p. 8 |
| ПЗУ | 2 Кбайт; la línea «в том числе, ПЗУ пользователя — 1 кбайт» está **tachada** | p. 8 |
| Interrupción | 1 vector | p. 8 |
| Software | programa «Монитор» | p. 8 |
| Niveles E/S | compatibles ТТЛ, disponibles en el ТЗЗ МI (campo de prototipos) | p. 8 |
| Alimentación | 220 V ±22 V, 50±1 Hz; salidas +5 V/0,70 A, +12 V/0,15 A, −5 V/0,20 A; ≤50 VA | p. 8 |
| Monitor | «занимает 1 Кбайт ПЗУ и использует последние 54 ячейки ОЗУ» | p. 16 |

### 1.1-bis El mapa de memoria real, y por qué no es el de tu encargo

Cuatro fuentes independientes convergen:

| Fuente | Qué dice |
|---|---|
| ПС p. 8, revisión ② (`РР1323-87`, 10.09.89) | ОЗУ **2** Кбайт (el «1» tachado a mano); ПЗУ 2 Кбайт; línea de «ПЗУ пользователя» tachada |
| ПЭ3 `РР3.055.472` л. 2 (ТЗЗ ПЦМ) | **`D24 КР537РУ8А`** — 1 ud. (SRAM 2 K×8) · **`D25 К573РФ2`** — 1 ud. (EPROM 2 K×8, equivalente al 2716) |
| ПС p. 25 (§4.5.1) | «оперативное запоминающее устройство (**D24**)»; «постоянное запоминающее устройство (**D25**)» — un chip cada uno |
| Fuente del monitor, p. 46 | `RAMEND EQU 1000H`, `BUFCD EQU 0FFAh`, `STKPTR EQU 0FCEh` |

De donde:

```
0x0000–0x03FF   Монитор            (К573РФ2, mitad baja)
0x0400–0x07FF   ПРОГРАММАТОР УМК   (К573РФ2, mitad alta)
0x0800–0x0FFF   ОЗУ  2 KB          (КР537РУ8А)
   0x0FCE–0x0FF9  variables del monitor + tabla de registros (LENTOS = 44)
   0x0FEE         tabla de vectores de interrupción en ОЗУ (USRSTTB)
   0x0FFA–0x0FFF  búfer de regeneración del display (BUFCD, 6 bytes)
   SP inicial del monitor = 0x0FCE
```

Es decir: **no hay 1 KB de «ПЗУ de usuario reservado»** — esa mitad lleva el
programador de EPROM — y **la RAM son 2 KB, no 1 KB**. El mapa de tu encargo
(y el de eax.me, que es su origen) describe la revisión anterior, con dos
`К573РФ1` de 1 KB y dos chips de ОЗУ de 512 B.

Lo trataré como **dos perfiles de máquina seleccionables** (`--rev=1` /
`--rev=2`, por defecto la 2, que es la que documenta el PDF), no eligiendo
uno y descartando el otro. El núcleo lleva la tabla de decodificación como
dato, no como `#ifdef`.

El museo de la informática (computer-museum.ru) atribuye el equipo al ПО «ВЭФ»
de Riga y lo fecha en 1983–1985, y afirma que УМК y УМПК-80 «использовали
одинаковое системное ПО "Монитор"». **Esa afirmación de equivalencia de
software no la voy a dar por buena**: ver DESCONOCIDOS.md §2.

### 1.2 Mapa de puertos — confirmado desde el fuente del monitor

Página 46 del PDF (hoja «−4−», MODULE PAGE 3 del listado), sección
`1. ОПИСАНИЕ ПОРТОВ ВВОДА/ВЫВОДА`, transcrito literalmente:

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
0FCE   BASETOS  EQU  RAMEND-LENTOS-6   ; БАЗА В ОЗУ
0FCE   STKPTR   EQU  BASETOS       ; ИСХ.ЗНАЧ. SP МОНИТ.
0FFA   BUFCD    EQU  RAMEND-6      ; БУФЕР РЕГЕНЕР. ИНДИК.
0FEE   USRSTTB  EQU  BASETOS+32    ; АДР.ВЕКТ.ПРЕР. В ОЗУ
0000 C34000     JMP  BOOT
```

Consecuencias directas:

- **F8/F9/FA/FB son los cuatro registros de un КР580ВВ55А (8255)**: A =
  selección de dígito *y* barrido de columnas de teclado (salida),
  B = segmentos (salida), C = filas de teclado (entrada), FB = registro
  de control.
- La palabra de control se escribe como `MVI A, NOT CNTRWRD` → `NOT 76H = 89H`
  (el listado imprime `3E89` en la columna OBJ, p. 48 del PDF). **0x89 =
  modo 0, PA salida, PB salida, PC entrada (ambas mitades)** — coherente al
  100 % con el uso descrito. Esto es una verificación cruzada fuerte: el
  fuente, los bytes objeto y la semántica del 8255 concuerdan.
- **Hay un quinto puerto que tu encargo no menciona: `0FCH`, «ПОРТ ПОШАГ.
  РЕЖ.»**, con `STEPWRD = 1`. Es el puerto por el que el monitor arma el modo
  paso a paso. Hay que emularlo.

### 1.3 El lazo de regeneración del display — el corazón del criterio 3

Página 72 del PDF (hoja «−30−», MODULE PAGE 29), transcrito literalmente:

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

Lecturas que esto fija:

- El barrido arranca en `B = 0x20` (bit 5) y **rota a la derecha** hasta
  `0x01`; al rotar `0x01` el acarreo se pone a 1 y reinicia. Seis posiciones,
  seis dígitos.
- `HL` empieza en `BUFCD = 0FFAh` y se incrementa con el barrido, luego
  `0FFA ↔ bit5`, `0FFB ↔ bit4`, … `0FFF ↔ bit0`. **Esto contradice
  aparentemente la convención de tu encargo** (`0b000001` = el de más a la
  izquierda). Ver DESCONOCIDOS.md §3: es la incógnita nº 1 y tiene solución
  experimental limpia.
- **El monitor sí apaga los segmentos antes de avanzar de dígito** (`MVI
  A,ERASE / OUT PORTB`). O sea: el propio monitor es la versión "correcta"
  del criterio de aceptación 3. Un núcleo que no modele la persistencia
  producirá fantasmeo *también en el monitor*, y eso es un test de regresión
  gratis.
- Máscara de teclado `74H` = bits 2, 4, 5, 6 de PORTC → **4 filas × 6 columnas
  = 24 teclas**, exactamente las «24 клавиш, из них 8 клавиш директивные, а
  16 — информационные» del ПС (p. 34).
- El antirrebote es por software: `TIME EQU 850` («ВРЕМЯ ДРЕБЕЗГА 10» ms) y
  una rutina `DELAY` en `035B`. **Un lazo de retardo calibrado en ciclos
  reales** — otra razón por la que el conteo de ciclos no es opcional.

### 1.4 Estructura del ПЗУ

- `0000` → `JMP BOOT` (`C3 40 00`), BOOT en `0040`.
- Vectores RST 1..6 en `0008`…`0030`, cada uno `LHLD USRSTTB+2n / PCHL`
  (redirección a tabla en ОЗУ). RST 7 (`0038`) es el manejador de
  interrupción del botón **ПР**.
- El monitor termina cerca de `03E7` (tablas `DS/DW`), o sea cabe en el 1 KB
  `0000–03FF`.
- **La segunda ROM (`0400–07FF`) no está "reservada": lleva el «ПРОГРАММАТОР
  УМК»**, que el listado incluye con `ORG 400H` (páginas 82–83 del PDF,
  hojas «−40−»/«−41−», código de `0400` a `0449`+). Es el programador de
  К573РФ1. Lo trataré como imagen ROM secundaria opcional, enchufable igual
  que la primera.

### 1.5 Directivas del monitor — formatos confirmados

Del `Руководство оператора` (`Р.Р.00004-01 34 01-1`, 11 hojas, páginas 85–96
del PDF) y del ПС (pp. 34–36):

| Directiva | Formato | Función |
|---|---|---|
| `П`  | `П XXXX ВП` | leer/modificar memoria |
| `РГ` | `РГ Y ВП` | leer/modificar registro |
| `СТ` | `СТ [A1] [A2] [A3] ВП` | ejecutar; A2/A3 = hasta **dos** puntos de ruptura |
| `КС` | `КС A1 A2 ВП` | suma de verificación (módulo 256, sin acarreo) |
| `ЗК` | `ЗК A1 A2 C ВП` | rellenar con constante |
| `ПМ` | `ПМ A1 A2 A3 ВП` | copiar bloque |
| `_`  | (espacio) | separador de parámetros / avance de byte |
| `ВП` | | fin de directiva |

Registros direccionables con `РГ`: `A B C D E H L P(признаков) SL SH PL PH`
(ПС p. 36 y Руководство p. 8). Tras `СБ` el display muestra `-`; ante error
de sintaxis muestra `?` y vuelve al estado inicial.

Botones y conmutadores en la Плата ПК (ПС p. 28): pulsadores `СБ` (S1),
`ШГ` (S2), `ПР` (S5); **conmutadores** `РБ/ШГ` (S3) y `КМ/ЦК` (S4). Es decir:
el enclavamiento que pides está en el documento, y la nomenclatura oficial es
`КМ/ЦК`, no `ММ/ЦИ`.

---

## 2. Lenguaje, dependencias y build

**Núcleo: C11 en modo independiente (freestanding).** Sólo `stdint.h`,
`stddef.h`, `stdbool.h` — cabeceras que el estándar garantiza sin biblioteca
de ejecución. Ni `stdio`, ni `stdlib`, ni asignación dinámica: el estado
completo de la máquina vive en un único `umk_machine_t` que el llamante
posee. Justificación:

- Cumple literalmente tu restricción («sin dependencias… ni siquiera de la
  biblioteca estándar»), cosa que en Rust exigiría `#![no_std]` + `alloc`
  a mano y aun así arrastra `cargo` y la red para cualquier crate del
  frontend, rompiendo «compilación reproducible con un solo comando» en
  una máquina sin acceso a crates.io.
- Un `umk_machine_t` POD hace que el entregable 6 (guardar/restaurar estado)
  sea un `memcpy` con cabecera versionada, no un problema de serialización.
- Es el mismo terreno en el que ya tengo verificado el camino a Windows
  nativo.

**Frontend: C11 con dos backends de plataforma detrás de una capa de ~200
líneas.**

- **Windows (objetivo primario): Win32 puro** — `user32` + `gdi32`, que van
  en el propio sistema. Cero dependencias externas, cero DLLs que copiar,
  el .zip portable arranca con doble clic.
- **POSIX: SDL2.** Es la ruta con menos fricción en Linux/macOS y no
  contamina el objetivo primario.

El panel se dibuja por software en un framebuffer del núcleo del frontend
(no del emulador) y se vuelca con `StretchDIBits` o `SDL_UpdateTexture`.
Así el dibujado es idéntico en las dos plataformas y testeable sin ventana.

**Headless: C11 + libc estándar solamente.** Sin SDL, sin Win32. Es el
binario que usan las pruebas automatizadas y CI.

**Build: CMake ≥ 3.16, un solo comando por plataforma.**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

Sin `FetchContent`, sin descargas en tiempo de compilación. En Windows,
MinGW-w64 (MSYS2 mingw64) como toolchain de referencia y MSVC soportado;
en Linux, gcc/clang con `libsdl2-dev`. Añado un `make.sh` y un `make.cmd`
de una línea para los que no quieran acordarse de nada.

---

## 3. Estructura del repositorio

```
umk80/
  core/                     # entregable 1 — freestanding, sin libc
    include/umk80/
      umk80.h               # API pública única, documentada
    src/
      i8080.c               # CPU: 256 opcodes + no documentados, ciclos exactos
      i8080_tables.h        # tablas de ciclos y banderas, generadas y comprobadas
      bus.c                 # decodificación de direcciones, espejeo
      i8255.c               # КР580ВВ55А
      display.c             # modelo de persistencia del multiplexado
      keyboard.c            # matriz 6×4 + enclavamientos
      panel.c               # LEDs АДРЕС/ДАННЫЕ/СОСТОЯНИЕ/alimentación
      step.c                # puerto 0FCH, РБ/ШГ, КМ/ЦК, ШГ
      machine.c             # ensamblado, reset, ПР, save/restore
  tools/
    umkasm/                 # ensamblador 8080 (entregable 4)
    umkdis/                 # desensamblador (entregable 4)
    umkrom/                 # transcripción del listado -> .bin + verificación
  frontend/                 # entregable 2
    platform_win32.c
    platform_sdl2.c
    render.c  keymap.c  main.c
  cli/                      # entregable 3 + 5 (depurador en modo texto)
  rom/
    monitor.asm             # fuente transcrito del listado (con comentarios rusos)
    monitor.obj.txt         # columna LOC/OBJ transcrita, para verificación cruzada
    programmer.asm          # ПРОГРАММАТОР УМК (ORG 400H)
    build_rom.md            # cómo regenerar monitor.bin de forma reproducible
  tests/
    cpu/                    # TST8080, 8080PRE, CPUTEST, 8080EXM + arnés CP/M
    display/                # criterio 3: fantasmeo vs HELLO limpio
    monitor/                # criterio 2: MVI A,AA / JMP + ПР + РГ A
    step/                   # criterio 4
  docs/
    umk_docs.pdf            # fuente primaria (26 MB) — no versionado, con SHA256
    pages/                  # páginas extraídas a PNG — no versionado
  PLAN.md  DESCONOCIDOS.md  README.md
```

---

## 4. La ROM del monitor — procedimiento

Ruta 2, con doble verificación independiente:

1. **Transcribir** las 39 páginas de listado (PDF pp. 45–83) a
   `rom/monitor.asm`. La calidad del escaneo es buena a resolución nativa
   (1019×1547 px ≈ 130 dpi): las dos páginas que ya he leído completas se
   transcriben sin ambigüedad. Transcribo también la columna `LOC/OBJ` a un
   fichero aparte, `rom/monitor.obj.txt`.
2. **Reconstruir** `monitor.bin` a partir de `LOC/OBJ` (herramienta
   `umkrom`). Huecos = `FF` y marcados como huecos, no rellenados en
   silencio.
3. **Reensamblar** `monitor.asm` con `umkasm` (entregable 4).
4. **Exigir igualdad byte a byte** entre (2) y (3). Cualquier discrepancia es
   un error de transcripción o de OCR y se resuelve volviendo al escaneo,
   no ajustando el fuente al binario. El registro de discrepancias
   resueltas va al repositorio.
5. **Verificación funcional**: la ROM así obtenida tiene que superar el
   criterio de aceptación 2 en el emulador. Si no lo hace, la transcripción
   no está terminada.

Salvaguardas de tu encargo que respeto: si me pasas un volcado binario real
de 1 KB, tiene prioridad absoluta (ruta 1) y lo uso para validar la
transcripción. El núcleo **no** contiene lógica de monitor en ningún caso; el
monitor sustituto nativo (ruta 3) sólo se escribirá si (4) fracasa, en un
módulo aparte, con la etiqueta `NO AUTÉNTICO` visible en el frontend.

**Nota de licencia**: el listado es documentación técnica soviética de 1986
de un fabricante que ya no existe. Lo trato como el resto de tus proyectos de
preservación. La imagen binaria y el fuente transcrito viven en `rom/`, no en
`core/`, y son sustituibles.

---

## 5. Fases y criterios de salida

Respeto tu orden: **la CPU y sus suites van primero, antes de tocar el panel.**

| Fase | Contenido | Criterio de salida |
|---|---|---|
| **F0** | Andamiaje: CMake, cabecera pública `umk80.h`, headless mínimo, CI local | `cmake --build` en Windows y Linux |
| **F1** | CPU 8080 completa: 256 opcodes incl. no documentados, banderas exactas (AC incluido), bit 1 del PSW a 1, conteo de ciclos por instrucción y por ciclo de máquina | **TST8080, 8080PRE, CPUTEST y 8080EXM pasan.** 8080EXM comparando los CRC esperados, no sólo «no falla» |
| **F2** | Bus, decodificación/espejeo, КР580ВВ55А, puerto `0FCH`, temporización a 2 MHz | El monitor arranca y llega a mostrar `-` tras `СБ`; lectura del decodificador `D15/D16` en el esquema (§DESCONOCIDOS 1) |
| **F3** | Transcripción y verificación de la ROM | `monitor.bin` reconstruido == reensamblado, byte a byte |
| **F4** | Frontend headless (entregable 3): cargar `.bin`/Intel HEX en dirección, correr N ciclos, volcar registros/memoria/display | Criterio de aceptación 2 pasa en headless |
| **F5** | Modelo de persistencia del multiplexado | **Criterio de aceptación 3**: el programa ingenuo difiere de HELLO, el corregido coincide |
| **F6** | Panel completo y frontend gráfico: 6 indicadores, 3 filas de LEDs (con la polaridad invertida de las de alimentación), teclado con ratón y con mapeo de teclas | Se opera el monitor con el ratón y se reproduce la secuencia de carga de tu encargo |
| **F7** | Depurador: puntos de ruptura, paso por instrucción y por ciclo de máquina, edición de registros y memoria; desensamblador | **Criterio de aceptación 4** |
| **F8** | Guardar/restaurar estado completo | Ciclo save→restore→continuar produce traza idéntica |
| **F9** | Ensamblador + empaquetado portable (zip Windows nativo y zip Linux, cada uno con su script) | Arranca en una VM limpia |

Commits pequeños, uno por unidad coherente, en español, con el `qué` y el
`por qué`.

### Modelo del multiplexado (F5) — cómo lo voy a hacer

No seis dígitos con estado propio, sino **un integrador temporal por
(dígito, segmento)**: 48 acumuladores de energía. Cada vez que avanza el
tiempo simulado, a cada par (dígito seleccionado en PORTA × segmento activo
en PORTB) se le suma el intervalo transcurrido; todos decaen con una
constante de persistencia del orden de la del indicador real. El frontend
dibuja la intensidad resultante. Con esto:

- el programa que no apaga antes de cambiar segmentos ilumina dos patrones
  sobre el mismo dígito durante el mismo intervalo → fantasmeo, sin ningún
  código especial que lo simule;
- el programa correcto produce un patrón estable;
- la prueba automatizada compara el **estado promediado** en una ventana de
  tiempo, como pides, sin depender de en qué instante se muestree.

La constante de persistencia es un parámetro con valor por defecto
documentado, no un número mágico (ver DESCONOCIDOS.md §5).

---

## 6. Trazabilidad entregables ↔ plan

| Entregable | Dónde |
|---|---|
| 1. Núcleo sin dependencias, API documentada | `core/`, F0–F2, F5 |
| 2. Frontend con panel completo | `frontend/`, F6 |
| 3. Modo headless CLI | `cli/`, F4 |
| 4. Desensamblador + ensamblador + .bin/Intel HEX | `tools/`, F3 y F9 |
| 5. Depurador | `cli/` + `frontend/`, F7 |
| 6. Save/restore | `core/machine.c`, F8 |
| 7. README + DESCONOCIDOS.md | raíz, desde ya |

---

## 7. Riesgos, ordenados por lo que pueden costar

1. **Transcripción de 39 páginas de listado.** Es el trabajo más largo y el
   más propenso a error silencioso. Mitigado por la doble verificación de §4:
   un error de transcripción rompe la igualdad OBJ↔reensamblado.
2. **Los esquemas están escaneados a poca resolución** (~2200×1600 px para
   dibujos de formato A1). Leer el decodificador de direcciones
   (`D15`, `D16` = К555ИД7) a nivel de pista puede no ser posible, y de ahí
   sale el espejeo. Mitigado: el espejeo no afecta a que el monitor
   funcione, sólo a la fidelidad en las zonas no pobladas. Ver
   DESCONOCIDOS.md §1.
3. **Orientación de los dígitos** (DESCONOCIDOS.md §3): afecta al criterio de
   aceptación 3. Tiene resolución experimental limpia, la explico allí.
4. **8080EXM tarda.** Se ejecuta en CI aparte, no en cada commit.

---

## 8. Lo que te pido antes de empezar

1. Visto bueno al lenguaje (C11 freestanding + Win32/SDL2) y al plan por fases.
2. Una decisión sobre DESCONOCIDOS.md §3 (orientación de los dígitos): puedo
   resolverlo yo por deducción del monitor, pero quiero que sepas que la
   convención de tu encargo y la que se deduce del código apuntan en sentidos
   opuestos.
3. Si tienes o puedes conseguir un volcado real de la ROM, dímelo ahora:
   cambia el orden de F3 y me ahorra el trabajo más largo del proyecto.
