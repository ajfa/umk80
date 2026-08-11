# Emulador del УМК-80

Emulador del **УМК-80** (Учебный микропроцессорный комплект), el banco
didáctico soviético que fabricó la asociación ВЭФ de Riga, referencia
`РР3.059.004` en maletín y `РР3.059.004-01` de sobremesa. Es una máquina de
enseñanza construida alrededor del КР580ВМ80А, el clon soviético del Intel
8080.

Emula la CPU con exactitud de ciclos, los seis indicadores de siete segmentos
multiplexados con su persistencia real, la matriz de teclado de 24 teclas, las
tres filas de LEDs del panel y el modo de ejecución paso a paso, por
instrucción o por ciclo de máquina.

**La ROM del monitor viene incluida**, reconstruida a partir del listado del
ensamblador que trae la documentación original. Ver §«La ROM» más abajo.

---

## Compilar

Hace falta un compilador C11 y nada más. El núcleo no usa siquiera la
biblioteca estándar.

### Windows (objetivo primario)

```
mingw32-make
```

El frontend gráfico usa Win32 puro (`user32` + `gdi32`), que va en el propio
sistema: el `.exe` arranca en una máquina limpia sin copiar ninguna DLL.
Probado con el gcc 16.2 de MSYS2 (`C:\msys64\mingw64\bin`).

### Linux y macOS

```bash
make
```

El frontend gráfico usa SDL2 (`libsdl2-dev` en Debian y derivados). Todo lo
demás — núcleo, herramientas, pruebas y modo sin ventana — compila sin
dependencia alguna.

Verificado en Ubuntu 22.04 (WSL2, gcc 11, SDL2 2.0.20): compila sin un solo
aviso, pasa las cuatro comprobaciones de aceptación y la verificación cruzada
de la ROM, y la ventana SDL2 se abre y se opera con el teclado. 8080EXM tarda
34 s ahí. En macOS no se ha probado.

### Con CMake

Se incluye un `CMakeLists.txt` equivalente para quien lo prefiera:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

---

## Ejecutar

### Panel gráfico

```
mingw32-make run
```

o directamente `build/umk80 --rom rom/monitor.bin`.

Se opera con el ratón sobre las teclas dibujadas o con el teclado del
anfitrión:

| Anfitrión | УМК-80 |
|---|---|
| `0`–`9`, `A`–`F` | teclas hexadecimales |
| `F1` … `F6` | `П` `РГ` `СТ` `КС` `ЗК` `ПМ` |
| espacio | separador de parámetros |
| Intro | `ВП` (fin de directiva) |
| Esc | `СБ` (сброс) |
| Retroceso | `ПР` (прерывание) |
| `F8` | `ШГ` (шаг) |
| `F9` | `РБ/ШГ` — enclava el modo paso a paso |
| `F10` | `КМ/ЦК` — el paso pasa a ser por ciclo de máquina |

También hay dos modos sin ventana, útiles para pruebas y para capturas:

```
build/umk80 --rom rom/monitor.bin --keys "p0800.3E.AA.C3.00.08.>" --shot panel.ppm
```

### Depurador y modo sin ventana

`umkcli` es a la vez el modo headless y el depurador:

```bash
build/umkcli --rom rom/monitor.bin                  # interactivo
build/umkcli --rom rom/monitor.bin -c "run 400000" -c display
```

Órdenes principales (`help` da la lista completa):

| Orden | Qué hace |
|---|---|
| `load <f> [dir]`, `loadhex <f>` | carga `.bin` o Intel HEX |
| `save`, `savehex` | vuelca memoria |
| `run [ciclos]`, `go <dir>` | ejecuta hasta el tope o hasta un punto de ruptura |
| `step [n]`, `cycle [n]` | paso por instrucción / por ciclo de máquina |
| `bp <dir>`, `bp list`, `bp del`, `bp clear` | puntos de ruptura |
| `regs`, `reg <r> <v>`, `mem`, `poke`, `dis` | inspección y edición |
| `key <nombre>`, `keys a,b,c` | pulsa teclas del panel |
| `display`, `panel` | estado de los indicadores y de los LEDs |
| `state save\|load <f>` | guarda y restaura la máquina entera |

### Herramientas sueltas

```bash
build/umkasm programa.asm programa.bin      # ensamblador 8080
build/umkdis programa.bin --org 0x800       # desensamblador
build/umkrom rom/monitor.lst rom/monitor.bin --asm rom/monitor.asm
```

---

## Comprobar que funciona

```
mingw32-make test
```

Ejecuta las cuatro comprobaciones que pide el encargo:

1. **Validación de la CPU** — TST8080, 8080PRE y CPUTEST. La cuarta,
   8080EXM, tarda varios minutos y va aparte: `make test-exm`. Las cuatro
   pasan, y 8080EXM con los 26 CRC correctos.
2. **El monitor real** — teclea la secuencia completa del encargo contra el
   monitor auténtico y comprueba que `РГ` + `A` muestra `A - AA`.
3. **Fidelidad del multiplexado** — el programa que no apaga los indicadores
   antes de cambiar los segmentos produce fantasmeo; el corregido muestra
   `HELLO` limpio.
4. **Paso a paso** — una pulsación de `ШГ` = una instrucción con `РБ/ШГ`
   enclavado, y = un ciclo de máquina si además lo está `КМ/ЦК`.

Y además `make verify-rom`, que se explica a continuación.

---

## La ROM

No existe volcado público de la ROM del УМК-80. Ésta se ha **reconstruido a
partir del listado del ensamblador** que incluye la documentación escaneada
(`Р.Р.00004-01 12 01-1`, «Системный монитор. Текст программы», 1986, литера
О₁, impreso con el ISIS-II 8080/8085 MACRO ASSEMBLER V4.0).

La transcripción está en [`rom/monitor.lst`](rom/monitor.lst), con la columna
de bytes objeto y la de fuente en paralelo, y se verifica por **dos vías
independientes que tienen que coincidir**:

```
make verify-rom
  ->  VERIFICACIÓN OK: reensamblado == columna OBJ, 2048 bytes idénticos
```

- `umkrom` reconstruye la imagen a partir de la columna `OBJ` del listado;
- `umkasm` reensambla la columna de fuente.

Para que un error de transcripción sobreviviera tendría que estar duplicado
exactamente en las dos columnas. Y encima la imagen resultante arranca y hace
funcionar el equipo, que es la tercera comprobación.

Si consigues un volcado binario real de 1 KB, tiene prioridad: cárgalo con
`--rom` y el núcleo lo usará tal cual.

**Contenido de la imagen** (2 KB, `0000h`–`07FFh`):

```
0000-03FF   Монитор
0400-044B   ПРОГРАММАТОР УМК   (grabador de EPROM, también en el listado)
```

---

## Mapa de la máquina

```
0000-03FF   ПЗУ, monitor
0400-07FF   ПЗУ, segunda mitad
0800-0FFF   ОЗУ  (2 KB en la revisión documentada)
   0FCE-0FF9   variables del monitor y tabla de registros
   0FEE        vectores de interrupción en ОЗУ
   0FFA-0FFF   búfer de regeneración del display (6 bytes)

F8   КР580ВВ55А puerto A — selección de indicador y barrido de columnas
F9   КР580ВВ55А puerto B — máscara de segmentos
FA   КР580ВВ55А puerto C — filas del teclado (máscara 74h)
FB   КР580ВВ55А control
FC   modo paso a paso (bit 0)
```

Hay dos perfiles de placa seleccionables. El de por omisión es la revisión
documentada (2 KB de ОЗУ); `--rev1` selecciona la anterior, con 1 KB espejado.
El porqué está en [DESCONOCIDOS.md §1](DESCONOCIDOS.md) y en
[PLAN.md §1.1-bis](PLAN.md).

---

## Estructura

```
core/        núcleo: CPU, bus, ВВ55, indicadores, teclado, panel, paso a paso
             — C11 independiente, sin libc, sin punteros en el estado
frontend/    panel gráfico: Win32 puro en Windows, SDL2 en POSIX
cli/         modo sin ventana y depurador
tools/       ensamblador, desensamblador, reconstructor de la ROM
rom/         transcripción del listado, fuente e imagen del monitor
tests/       las cuatro comprobaciones de aceptación
docs/        procedencia de las fuentes documentales
```

El núcleo no contiene lógica de monitor ni de interfaz: la ROM es enchufable y
el panel se dibuja fuera. `umk_machine_t` no tiene punteros, así que guardar y
restaurar el estado completo es copiar la estructura.

---

## Documentos

- **[PLAN.md](PLAN.md)** — el plan de trabajo, con lo que está verificado
  documentalmente y de dónde sale cada dato.
- **[DESCONOCIDOS.md](DESCONOCIDOS.md)** — todo lo que **no** se ha podido
  confirmar, qué se sabe, por qué no basta y cómo se ha resuelto o cómo se
  propone resolverlo. Se lee antes de fiarse de cualquier detalle fino.
- **[docs/FUENTES.md](docs/FUENTES.md)** — de dónde sale cada documento, con
  su SHA256.

## Licencia y procedencia

El código del emulador es obra propia. La ROM del monitor y su fuente
proceden de documentación técnica soviética de 1986 de un fabricante que ya
no existe, y se incluyen con fines de preservación; viven en `rom/`, separados
del núcleo, y son sustituibles.

El panel está dibujado a mano a partir del plano del fabricante (Рис. 2 del
ПС). Las fotografías de `docs/ref/`, que sirvieron para la paleta y las
proporciones, son de sus autores y **no** forman parte de lo que se
distribuye.
