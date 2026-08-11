# DESCONOCIDOS — УМК-80

Todo lo que **no** puedo confirmar documentalmente a fecha de 2026-08-11, con
lo que sí sé, por qué no basta, y cómo propongo resolverlo. Nada de esto está
implementado todavía.

Convención: **[A]** = bloquea un criterio de aceptación · **[B]** = afecta a
la fidelidad pero no bloquea · **[C]** = cosmético o documental.

---

## 1. [B] Decodificación de direcciones y espejeo

**Lo que sé.** El ПС (p. 25) dice que el decodificador de direcciones son
`D15`, `D16` (dos К555ИД7, decodificadores 3→8) más `D5.1`, `D5.2` (mitades
de un К555ЛН2). Hay 2 KB de ПЗУ en `0000–07FF` y 2 KB de ОЗУ en
`0800–0FFF`, es decir dos bloques contiguos de 2 KB. Existe además una
«схема блокировки памяти» (`D23.2`, `D14`, `D9.2`) — bloqueo de memoria
asociado al modo paso a paso.

**Lo que no sé.** Qué líneas de dirección entran en los ИД7. Con dos
decodificadores 3→8 hay 16 salidas, y sólo dos se usan. Lo más probable es
que se decodifiquen `A11–A13` (granularidad de 2 KB, 16 KB de espacio
decodificado, y espejo cada 16 KB en los 64 K), pero **es una inferencia,
no un dato**. Tampoco sé si las direcciones `0x1000–0xFFFF` responden con
espejos, con el bus flotante, o con `FF`.

**Cómo lo resuelvo.** Recortar la zona de `D15`/`D16` del esquema
`РР3.055.472 Э3` (página 104 del PDF, 2230×1612 px) y leerla ampliada. Si no
es legible, queda como parámetro configurable con el espejo cada 16 KB por
defecto **y marcado como inferido en el README y en la salida de `--info`**.

**Por qué no bloquea.** Ni el monitor ni ninguno de los programas de tus
criterios de aceptación tocan direcciones fuera de `0000–0FFF`.

---

## 2. [B] ¿Es el monitor del УМК el mismo que el del УМПК-80?

**Lo que sé.** computer-museum.ru afirma que ambos «использовали одинаковое
системное ПО "Монитор"». El emulador de GalaxyShad trae en `data/` varios
binarios (`os.bin` 1712 B, `old.bin` 1712 B, `scaned-os-fixed.bin` 2048 B,
`scaned-os.bin` 3072 B) y su README dice que el búfer de siete segmentos
está en `0BFA–0BFF`.

**Lo que no sé.** Si esos binarios tienen algo que ver con el УМК. El
búfer del monitor del УМК está en **`0FFA–0FFF`**, no en `0BFA–0BFF`. Eso
solo ya demuestra que **no son el mismo binario**, sea cual sea el
parentesco. La afirmación del museo es, como mucho, «funcionalmente
equivalente».

**Cómo lo resuelvo.** No asumir nada: reconstruyo la ROM del УМК desde su
propio listado (PLAN §4). Los binarios de GalaxyShad los uso sólo como
banco de pruebas de contraste del núcleo de CPU — nunca como fuente de
verdad del УМК. Tu encargo ya lo advertía y estoy de acuerdo.

---

## 3. [A] Orientación de los seis dígitos — la incógnita nº 1

**El conflicto.** Tu encargo (y eax.me) dicen: `0b000001` = indicador de más
a la izquierda. Pero el lazo de regeneración del monitor (PDF p. 72) arranca
en `NMBIND = 0b00100000` (bit 5), rota **a la derecha** hasta el bit 0, y
recorre el búfer `BUFCD` **hacia adelante** (`INX H`). Luego:

```
0FFA ↔ bit5      0FFB ↔ bit4      0FFC ↔ bit3
0FFD ↔ bit2      0FFE ↔ bit1      0FFF ↔ bit0
```

Si el bit 0 fuera el dígito de más a la izquierda, el monitor estaría
escribiendo su búfer de derecha a izquierda, cosa rarísima para un display
que muestra `ДДДД ДД` (dirección de 4 dígitos a la izquierda, dato de 2 a la
derecha). Lo natural es lo contrario: **bit 5 = el de más a la izquierda**.

**Por qué importa.** Tu criterio de aceptación 3 dice que la versión
corregida debe mostrar `HELLO`. Ese programa arranca con `B = 0x01` y rota a
la **izquierda** (`RLC`), leyendo `H,E,L,L,O` en orden ascendente desde
`081B`. Con la convención de tu encargo (bit0 = izquierda) sale `HELLO`
leyéndose de izquierda a derecha. Con la convención que se deduce del
monitor, saldría **espejado**: `_OLLEH`. Las dos convenciones no pueden ser
ciertas a la vez.

**Cómo lo resuelvo.** No por deducción: por consistencia forzada.
1. Transcribo la rutina que formatea la dirección de 4 dígitos y el dato de
   2 (`CDADR`/`COBYTE`, en torno a `0280–02B0`) y miro **en qué orden llena
   `BUFCD`**. Eso fija la orientación sin ambigüedad, porque el formato de
   pantalla `АДРЕС ДАННЫЕ` está fotografiado en el ПС (Рис. 2, p. 17) y
   descrito en el Руководство оператора.
2. Contrasto con la foto del panel del artículo de dlinyj en Habr.

**Mientras tanto.** El núcleo lleva la orientación como **parámetro
explícito** (`umk_display_set_digit_order`), y las dos pruebas del criterio 3
se ejecutan con la orientación que resulte de (1). Si sale la contraria a la
de tu encargo, te lo digo antes de tocar los tests, no después.

---

## 4. [B] Mapa exacto de la matriz de teclado 6×4

**Lo que sé.** 24 teclas (8 directivas + 16 informativas), ПС p. 34. Se leen
por `IN PORTC` con máscara `74H` → filas en los bits 2, 4, 5 y 6. Las
columnas son las mismas seis líneas de `PORTA` que seleccionan el dígito. La
rutina `CONV` (PDF p. 73) convierte el código leído a ASCII con:

```
; КОД ASCII ЦИФР. СИМВОЛА:      (COD AND 0EFH) / 8  + АДР.ИНДИК-РА / 8
; КОД ASCII ФУНКЦИОНАЛЬНОГО:    (COD AND 0EFH) / 16 + АДР.ИНДИК-РА - 1
```

y decide entre las dos por el bit 4 (`ANI 10H`).

**Lo que no sé.** Qué tecla física está en cada intersección (columna,
fila). La fórmula de `CONV` lo determina por completo, pero hay que
transcribir la tabla de códigos que hay a continuación (`ТАБЛИЦА КОДОВ
СИМВОЛОВ`, en torno a `037C–03A0`, PDF p. 79) y cruzarla con la disposición
física del panel.

**Cómo lo resuelvo.** Transcripción de esa tabla + Рис. 2 del ПС (p. 17,
dibujo del panel frontal) + fotos del panel real en los artículos de Habr.
Es trabajo, no incertidumbre: la información está.

---

## 5. [B] Constante de persistencia del indicador

**Lo que sé.** El display es de seis dígitos multiplexados, gobernado por
`D20`, `D21`, `D22` y los transistores `VT1…VT6` (ПС p. 25). El lazo de
regeneración del monitor tarda unos pocos cientos de ciclos por dígito a
2 MHz.

**Lo que no sé.** El tipo exacto de indicador y por tanto su persistencia
fosforescente/térmica real. El ПЭ3 de la placa ПИ (páginas 108–110 del PDF)
debería listarlo (АЛС318, АЛС324 o similar), pero aún no lo he leído.

**Cómo lo resuelvo.** Leer el ПЭ3 de la placa ПИ. Si no aparece, uso una
constante de decaimiento que reproduzca el comportamiento observable
descrito (fantasmeo visible en el programa ingenuo, estabilidad en el
corregido) y la dejo como parámetro documentado con su valor por defecto
justificado, no como número mágico. **Ninguna prueba automatizada dependerá
del valor exacto**: la comparación del criterio 3 es sobre el estado
promediado, que es robusto a la constante.

---

## 6. [B] Frecuencia exacta del reloj

**Lo que sé.** «≈ 2 MHz» (eax.me). El generador es un КР580ГФ24 (`D1`), que
divide el cristal por 9.

**Lo que no sé.** El valor del cristal. Un ГФ24 con cristal de 18,00 MHz da
exactamente 2,000 MHz, que es lo típico; pero el cristal no está en el
perechen que he leído (está en el ПЭ3, listo `ZQ`), y no lo he verificado.

**Por qué importa poco.** El antirrebote (`TIME = 850`) y el brillo del
display dependen de la frecuencia, pero de forma proporcional y sin efectos
de umbral. Un 1 % de error no cambia ningún comportamiento observable.

**Cómo lo resuelvo.** Leer el `ZQ` en el ПЭ3 del ТЗЗ ПЦМ (páginas 105–107).
Por defecto 2,000 MHz, configurable.

---

## 7. [B] Semántica del puerto `0FCH` (ПОРТ ПОШАГ. РЕЖ.)

**Lo que sé.** Existe, el monitor le escribe `STEPWRD = 1`, y el ПС (p. 25)
describe una «схема пошагового выполнения программ» con diez elementos
lógicos (`D14`, `D19`, `D8.2`, `D23.4`, `D23.3`, `D13.2`, `D5.4`, `D3.9`,
`D10.1`, `D5.5`) y una «схема блокировки памяти» separada. El ПС p. 16
explica el mecanismo: el conmutador `РБ/ШГ` en `ШГ` pasa la СУ al estado
«ожидание» tras cada paso, y `КМ/ЦК` elige si el paso es por instrucción
(lectura del primer byte del código de operación) o por ciclo de máquina.

**Lo que no sé.** Qué bits exactos del puerto `0FCH` hacen qué, y si es
sólo escritura o también lectura. Tampoco cómo interactúa la «блокировка
памяти» con el paso a paso (sospecho que congela la ОЗУ para que el propio
monitor no se corrompa mientras el usuario avanza).

**Por qué importa.** Es tu criterio de aceptación 4.

**Cómo lo resuelvo.** Transcribir las rutinas del monitor que escriben en
`DBGPORT` — el listado las tiene y las comenta — y cruzarlas con la
descripción funcional del ПС p. 16, que es explícita sobre el comportamiento
observable. El comportamiento observable **sí** está documentado; lo que
falta es el detalle eléctrico, que puedo modelar de la forma más simple que
lo reproduzca.

---

## 8. [B] Polaridad de los LEDs de alimentación

**Lo que dices.** LED encendido = esa tensión **falta**.

**Lo que sé.** El ПС describe una «устройство индикации аварии» (p. 18) y un
esquema de bloqueo de los estabilizadores; los tres LEDs son de avería, no
de presencia. Eso es **coherente** con lo que dices, pero no lo he leído
literalmente en la forma «горит = нет напряжения».

**Cómo lo resuelvo.** Lo implemento con **tu** polaridad, que es la que
tiene sentido para un indicador de avería, y lo dejo anotado aquí hasta
poder citar la frase exacta del ПС §4.3 (p. 18) o del esquema del bloque de
alimentación (página 98 del PDF).

---

## 9. [B] Instrucciones no documentadas del КР580ВМ80А

**Lo que sé.** Los ocho opcodes no documentados del 8080 (`08`, `10`, `18`,
`20`, `28`, `30`, `38` = `NOP`; `CB` = `JMP`; `D9` = `RET`; `DD`/`ED`/`FD` =
`CALL`) están bien caracterizados para el Intel 8080, y 8080EXM los ejercita.

**Lo que no sé.** Si el clon soviético КР580ВМ80А se comporta igual en todos
ellos. No he encontrado ninguna fuente que documente una divergencia, pero
«no he encontrado» no es «no existe».

**Cómo lo resuelvo.** Implemento la semántica del Intel 8080 (que es lo que
exigen las suites de tu criterio 1) y lo anoto. Si aparece documentación de
una divergencia, es un parámetro más del perfil de máquina.

---

## 10. [C] Nomenclatura de los conmutadores

Tu encargo dice `ММ/ЦИ` «también documentado como `КМ/ЦК`». La
documentación oficial (ПС p. 28, lista de la Плата ПК) dice **`КМ/ЦК`**
(`S4`), junto a `РБ/ШГ` (`S3`), `СБ` (`S1`), `ШГ` (`S2`) y `ПР` (`S5`). Usaré
`КМ/ЦК` como nombre canónico y aceptaré `ММ/ЦИ` como alias en la
configuración de teclas.

---

## 11. [C] Qué lleva realmente el zócalo de ПЗУ en unidades reales

El listado incluye un «ПРОГРАММАТОР УМК» con `ORG 400H`. La revisión ② del
ПС tachó la línea de «ПЗУ пользователя», lo que sugiere que en la revisión
final el segundo kilobyte lo ocupa el programador de fábrica. Pero **no sé si
todas las unidades salieron con él grabado**. Lo trato como imagen opcional:
si no se carga, esa mitad lee `FF`.

---

## 12. [C] Chips de memoria según la unidad física

Tu encargo dice: «la documentación declara 2 × КР541РУ2, las unidades reales
llevan К537РУ13». La documentación que yo tengo **no dice ninguna de las
dos**: dice `D24 КР537РУ8А` (una unidad, 2 K×8) y `D25 К573РФ2` (una unidad,
2 K×8). Son tres inventarios distintos para el mismo equipo, lo que confirma
que hubo al menos dos revisiones de placa. No afecta a la emulación más allá
de los tamaños, que ya están cubiertos por los dos perfiles de máquina del
PLAN §1.1-bis, pero conviene que sepas que el dato que me diste y el mío no
coinciden y que **ninguno de los dos es necesariamente erróneo**.

---

## 13. [C] Fecha y variantes del equipo

computer-museum.ru fecha el УМК en 1983–1985. La documentación que tengo está
firmada en 1987–1989 y el listado del monitor lleva `1986, литера О₁`. El
artículo de tu encargo lo fecha en 1988. Probablemente todo sea cierto para
distintas revisiones. Sin relevancia técnica; lo anoto para no repetir una
fecha concreta como si fuera la única.
