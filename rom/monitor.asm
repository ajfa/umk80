; УМК-80 monitor source, extracted from rom/monitor.lst.
; DO NOT EDIT: regenerate with  umkrom rom/monitor.lst ... --asm
; Original: Р.Р.00004-01 12 01-1, «Системный монитор. Текст
; программы», 1986, литера О1, ISIS-II 8080/8085 MACRO ASSEMBLER.

 ; 1. ОПИСАНИЕ ПОРТОВ ВВОДА/ВЫВОДА
         ORG   0
 PORTA   EQU   0F8H          ; ПОРТ АДРЕСА
 PORTB   EQU   0F9H          ; ПОРТ ДАННЫХ
 PORTC   EQU   0FAH          ; ПОРТ СОСТОЯНИЯ
 CNTRRG  EQU   0FBH          ; ПОРТ УПРАВЛ. БИС
 DBGPORT EQU   0FCH          ; ПОРТ ПОШАГ. РЕЖ.
 CNTRWRD EQU   76H           ; УПРАВЛЯЮЩЕЕ СЛОВО
 STEPWRD EQU   1             ; УСТ. ПОШАГ. РЕЖИМА
 NMBIND  EQU   00100000B     ; N ИНДИКАТ. N 5
 TIME    EQU   850           ; ВРЕМЯ ДРЕБЕЗГА 10
 ERASE   EQU   0             ; СБРОС ИНДИКАЦИИ
 SPACE   EQU   6             ; КОД СИМВ.-ПРОБЕЛ
 CR      EQU   7             ; КОД СИМВ.-ВОЗВРАТ
 TYPEAD  EQU   1             ; ТИП ДАННЫХ-ADDRESS
 TYPEBT  EQU   0             ; ТИП ДАННЫХ - BYTE
 RAMEND  EQU   1000H         ; ВЕРХНЯЯ ГРАН. ОЗУ
 LENTOS  EQU   44            ; ДЛ.ТАБЛ.ИСХ.ЗНАЧ. РЕГ.И ВЕКТ.ПРЕРЫВ.
 BASETOS EQU   RAMEND-LENTOS-6   ; БАЗА В ОЗУ
 STKPTR  EQU   BASETOS       ; ИСХ.ЗНАЧ. SP МОНИТ.
 BUFCD   EQU   RAMEND-6      ; БУФЕР РЕГЕНЕР. ИНДИК.
 USRSTTB EQU   BASETOS+32    ; АДР.ВЕКТ.ПРЕР. В ОЗУ
        JMP   BOOT
 ; ОБРАБОТЧИКИ ПРЕРЫВАНИЙ ПОЛЬЗОВАТЕЛЯ.
 ; 0-Й И 7-Й УРОВНИ ПРЕРЫВАНИЙ
 ; ИСПОЛЬЗУЮТСЯ ПРОГРАММОЙ МОНИТОР.
         ORG   08H
       LHLD  USRSTTB       ; RST 1
         PCHL                ; ОБР. 1 УРОВЕНЯ
         ORG   10H
       LHLD  USRSTTB+2     ; RST 2
         PCHL                ; ОБР. 2 УРОВЕНЯ
         ORG   18H
       LHLD  USRSTTB+4     ; RST 3
         PCHL                ; ОБР. 3 УРОВЕНЯ
         ORG   20H
       LHLD  USRSTTB+6     ; RST 4
         PCHL                ; ОБР. 4 УРОВЕНЯ
         ORG   28H
       LHLD  USRSTTB+8     ; RST 5
         PCHL                ; ОБР. 5 УРОВЕНЯ
         ORG   30H
       LHLD  USRSTTB+10    ; RST 6
         PCHL                ; ОБР. 6 УРОВЕНЯ
 ; 3. ИНИЦИАЛИЗАЦИЯ ПРОГРАММЫ МОНИТОР
         ORG   38H
 ; ОБРАБОТКА ПРЕРЫВАНИЯ 7-ГО УРОВНЯ
         PUSH  PSW
         XRA   A             ; СБР.ПОШАГОВЫЙ РЕЖИМ
         OUT   DBGPORT
         POP   PSW
       JMP   RESTART
 BOOT:
 ; ИНИЦИАЛИЗАЦИЯ СИСТЕМЫ МОНИТОР
 ; УСТАНОВКА ИСХОДНЫХ ЗНАЧЕНИЙ РЕГИСТРОВ
       LXI   SP,STKPTR     ; УСТ.SP МОНИТОРА
       LXI   H,BASETOS     ; АДР.РЕГ.В ОЗУ
       LXI   B,TOS         ; ИСХ.ЗНАЧ.РЕГ.
         MVI   D,LENTOS
 MOVLP:
         LDAX  B
         MOV   M,A
         INX   H
         INX   B
         DCR   D
       JNZ   MOVLP
 ; ИСХОДНОЕ ПРОГРАММИРОВАНИЕ БИС
         MVI   A,NOT CNTRWRD
         OUT   CNTRRG
       CALL  ERSIND        ; ГАШЕНИЕ ИНДИКАЦ.
 ; 4. СТАРТ ПРОГРАММЫ МОНИТОР.
 START:
 ; ИНИЦИАЛ-Я БУФЕРА ВЫВОДА И ВЫВОД СИМВОЛА
 ; "-"  ГОТОВ К ПРИЕМУ ДИРЕКТИВЫ
 ; ПРИЕМ С КОНСОЛИ КЛЮЧЕВОГО СЛОВА ДИРЕКТИВЫ
 ; ПЕРЕДАЧА УПРАВЛЕНИЯ МОДУЛЮ - ЗАДАНИЮ.
         EI
 ; УСТАНОВКА УКАЗАТЕЛЯ СТЕКА МОНИТОРА
       LXI   SP,STKPTR
 ; ЕСЛИ МЛ. ИНДИКАТОР ПОГАШЕН, ВЫВЕСТИ "-"
 ; СЛЕВА, ИНАЧЕ ВЫВОД "-" СПРАВА
       LXI   H,BUFCD+5
         MOV   A,M
         ORA   A
         MVI   B,5           ; АДР.СТ.ИНДИКАТОРА
       JZ    $+5
         MVI   B,0           ; АДР.МЛ.ИНДИКАТОРА
         MVI   C,40H
       CALL  CONC
 ; АДРЕС ВОЗВРАТА
       LXI   H,START
         PUSH  H
 ; ПРИЕМ КЛЮЧЕВОГО СИМВОЛА
       CALL  CI
       CALL  ERSIND
 ; КОНТРОЛЬ
         CPI   6
       JNC   ERROR
 ; ВЫБОР ЗАДАНИЯ В C T B L
       LXI   H,CTBL
         ADD   A
         MOV   E,A
         MVI   D,0
         DAD   D
 ; <HL> - АДРЕС ЗАДАНИЯ
         MOV   A,M
         INX   H
         MOV   H,M           ; СТ. БАЙТ АДРЕСА
         MOV   L,A           ; МЛ. БАЙТ АДРЕСА
 ; ПЕРЕДАЧА УПРАВЛЕНИЯ
         PCHL
 ; 5. РЕАЛИЗАЦИЯ ДИРЕКТИВ ПРОГРАММЫ МОНИТОР
 REPLRG:
 ; 5.1. МОДИФИКАЦИЯ СОДЕРЖИМОГО РЕГИСТРОВ
       CALL  ERSIND        ; ПОГАСИТЬ ИНДИКАЦИЮ
       CALL  CI            ; ПРИЕМ ИДЕНТ-РА РЕГ.
         CPI   '4'           ; КОНТРОЛЬ
       JC    ERROR
         CPI   'A'           ; ЕСЛИ CODE>='A',
       JC    $+5           ; ТО CODE-7
         SBI   7
 ; ОПРЕДЕЛЕНИЕ АДРЕСА ХРАНЕНИЯ ЗАДАННОГО
 ; РЕГИСТРА И ФИЗИЧ. КОДОВ СИМВОЛОВ ИДЕНТИ-
 ; ФИЦИРУЮЩИХ РЕГИСТР ПО ТАБЛ. TBLRG
 ; СМЕЩЕНИЕ В ТАБЛ.-(КОД ASCII AND 0FH) * 4
       LXI   H,TBLRG-20
         ANI   0FH
         MOV   E,A
         ADD   A
         ADD   A
         ADD   E
         MVI   D,0
         MOV   E,A
         DAD   D
 ; <C> - КОД ИДЕНТИФИКАТОРА
         MVI   B,5
 IDTOUT:
         MOV   A,M
         ORA   A
       JZ    $+9
         MOV   C,A
         PUSH  H
       CALL  CONC
         POP   H
         INX   H
         DCR   B
         MOV   A,B
         CPI   2
       JNZ   IDTOUT
         PUSH  H
         MVI   C,40H
       CALL  CONC
         POP   H
 ; <HL> - АДРЕС Я.П. ХРАНЕНИЯ РЕГИСТРА
         MOV   A,M
         INX   H
         MOV   H,M
         MOV   L,A
         MOV   C,M           ; ТЕК.ЗНАЧ.РЕГ.
         PUSH  H
       CALL  COBYTE        ; ПЕЧАТЬ БАЙТА
       CALL  PCHK          ; ЕСЛИ РАЗДЕЛ.
         POP   H
       JC    ERSIND        ; КОНЕЦ
       JZ    REPLRG        ; СЛЕД.РЕГИСТР
 ; ПРИНЯТЬ НОВОЕ ЗНАЧ. РЕГИСТРА
         PUSH  H
       CALL  ERSBT
         MVI   C,TYPEBT
       CALL  PARM1
         MOV   A,L
         POP   H
         MOV   M,A
       JNC   REPLRG
       JMP   ERSIND        ; ПОГАСИТЬ ИНДИКАЦИЮ
 ; 5.2. МОДИФИКАЦИЯ СОДЕРЖИМОГО ЯЧЕЙКИ ПАМЯТИ
 REPLM:
 ; ПРИЕМ АДРЕСА Я.П.
         MVI   C,TYPEAD
       CALL  PARAM
 COMEMLP:
 ; ПЕЧАТЬ СОДЕРЖ. Я.П.
         MOV   C,M
         PUSH  H
       CALL  COBYTE
 ; ПРИЕМ СИМВОЛА
       CALL  PCHK          ; ЕСЛИ РАЗДЕЛ., СЛЕД.ЯЧ
         POP   H
       JC    ERSIND        ; ЗАКОНЧИТЬ ДИРЕКТИВУ
       JZ    MEMNEXT
 ; ПРИНЯТЬ НОВОЕ ЗНАЧЕНИЕ СОДЕРЖ. Я.П.
         PUSH  H
       CALL  ERSBT
         MVI   C,TYPEBT
       CALL  PARM1
         MOV   A,L
         POP   H
         MOV   M,A
       JC    ERSIND        ; ЗАКОНЧИТЬ
 MEMNEXT:
 ; ПЕРЕЙТИ К СЛЕД. Я.П.
         INX   H
         MOV   B,H
         MOV   C,L
         PUSH  H
       CALL  COADR
         POP   H
       JMP   COMEMLP
 ; 5.3. ВЫПОЛНЕНИЕ ПРОГРАММЫ ПОЛЬЗОВАТЕЛЯ
 GOTO:
 ; ПРЕДУСМОТРЕНА ВОЗМОЖНОСТЬ УСТАНОВКИ
 ; ДО 2-Х ТОЧЕК ОСТАНОВА
       LXI   H,BASETOS+EXIT-TOS  ; АД.EXIT
         XTHL                ; В СТЕКЕ АДРЕС EXIT
       CALL  PCHK
       JZ    G00           ; ВЫП.С ТЕК.АДРЕСА
         MVI   C,TYPEAD
       CALL  PARM1         ; ПРИЕМ НАЧ.АДРЕСА
       SHLD  BASETOS+PCLOC+1-TOS ; ЗП.АДР.
                             ; ВХОДА В ПРОГ.ПОЛЬЗ.
 G00:
         RC                  ; ВЫПОЛНИТЬ
       LXI   D,2           ; ПР.АДР.2-Х ТЧК.ОСТ.
 G01:
       CALL  ERSADR        ; ПОГАСИТЬ АДР.ИНДИК.
       LXI   B,240H        ; ВЫВОД "-" НА 2 ИНД.
         PUSH  D
       CALL  CONC
       CALL  PARAM         ; ПРИЕМ АДРЕСА
         POP   D
         PUSH  H
         INR   D             ; КОЛ-ВО ПАРАМЕТРОВ+1
       JC    G02           ; ПАРАМЕТРЫ ПРИНЯТЫ
         DCR   E
       JNZ   G01
 G02:
       JNC   ERROR         ; > 2-Х ТОЧЕК ОСТ.
 ; ЗАПОМНИТЬ АДРЕСА ТОЧЕК ОСТАНОВА И
 ; ЯЧЕЕК ПАМЯТИ В СПЕЦИАЛЬНОЙ ОБЛ.ОЗУ
 ; УСТАНОВИТЬ В НИХ ТОЧКИ ПРЕРЫВАНИЯ
       LXI   H,BASETOS+TLOC-TOS
 G03:
         POP   B             ; АДРЕС ТОЧКИ ОСТАНОВА
         MOV   M,C           ; СОХРАНИТЬ
         INX   H
         MOV   M,B
         INX   H
         LDAX  B             ; СОДЕРЖ.ТОЧКИ ОСТАН.
         MOV   M,A           ; СОХРАНИТЬ
         MVI   A,0FFH        ; RST7
         STAX  B             ; УСТ. ПРЕРЫВАНИЕ
         INX   H
         DCR   D
       JNZ   G03           ; 2 ТОЧКА ОСТАНОВА
         RET
 ; 5.4. ЗАПОЛНЕНИЕ МАССИВА ОЗУ КОНСТАНТОЙ
 FILE:
         MVI   C,2           ; ПРИНЯТЬ НАЧ. И КОН.
       CALL  CIADR         ; АДРЕСА ОБЛАСТИ ОЗУ
         MVI   C,TYPEBT
       CALL  PARAM         ; ПРИЕМ ДАННЫХ
         MOV   C,L
         POP   D             ; КОН.АДРЕС
         POP   H             ; НАЧ.АДРЕС
       CALL  COMPA1
       JC    ERROR
 FILELP:
         MOV   M,C
       CALL  COMPA         ; ЕСЛИ HL > DE,
       JNC   FILELP        ; CARRY = 1
       JMP   ERSIND        ; ПОГАСИТЬ ИНД.
 ; 5.5. КОПИРОВАНИЕ ОБЛАСТЕЙ ПАМЯТИ
 MOVE:
         MVI   C,3           ; ПРИЕМ ПАРАМЕТРОВ
       CALL  CIADR         ; ПЕРЕМЕЩЕНИЯ
         POP   B             ; НАЧ.АДР. - КУДА
         POP   D             ; КОН.АДРЕС ИСТОЧНИКА
         POP   H             ; НАЧ.АДРЕС ИСТОЧНИКА
       CALL  COMPA1
       JC    ERROR
 MOVELP:
         MOV   A,M
         STAX  B
         INX   B
       CALL  COMPA
       JNC   MOVELP
       JMP   ERSIND
 ; 5.6. ПОДСЧЕТ КОНТРОЛЬНОЙ СУММЫ
 CHSUM:
 ; ПОДСЧЕТ КС ЗАДАННОЙ ОБЛАСТИ ОЗУ
 ; КС - ПРЕДСТАВЛЯЕТ СОБОЙ СУММУ ВСЕХ ЯЧЕЕК
 ; ПАМЯТИ БЕЗ УЧЕТА ПЕРЕНОСА
         MVI   C,2           ; ПРИЕМ НАЧ.И КОН.
       CALL  CIADR         ; АДРЕСОВ
         POP   D
         POP   H
       CALL  COMPA1
       JC    ERROR
         MVI   C,0           ; ИСХОДНОЕ ЗНАЧ. КС
 CSUMLP:
         MOV   A,C
         ADD   M
         MOV   C,A
       CALL  COMPA
       JNC   CSUMLP
       CALL  COBYTE        ; ВЫВОД ЗНАЧ. КС
       JMP   ERSADR        ; ПОГАСИТЬ АД.ИНД.
 ; 6. ОБРАБАТЫВАЮЩИЕ ПРОГРАММЫ.
 PLLOC   EQU   PCLOC+1
 PHLOC   EQU   PLLOC+1
 LLOC    EQU   HLLOC+1
 HLOC    EQU   LLOC+1
 SLLOC   EQU   SPLOC
 SHLOC   EQU   SLLOC+1
 RESTART:
 ; ОБРАБОТКА ПРЕРЫВАНИЯ 7-ГО УРОВНЯ
 ; - СОХРАНИТЬ ТЕКУЩЕЕ СОСТОЯНИЕ ПРОЦЕССОРА
 ; - ВОССТАНОВИТЬ ИСХ.ЗНАЧ.ТОЧЕК ПРЕРЫВАНИЯ
         DI
 ; ЕСЛИ ПРЕРВАНА РАБОТА ПРОГРАММЫ МОНИТОР,
 ; ПРЕРЫВАНИЕ ИГНОРИРОВАТЬ
         XTHL                ; HL-АДРЕС Т.ПРЕРЫВ.
         DCX   H             ; АДРЕС ТОЧКИ ОСТАНОВА
         PUSH  D
         PUSH  PSW
       LXI   D,PCEND
         MOV   A,L
         SUB   E
         MOV   A,H
         SBB   D
       JC    ERROR         ; ПРЕРВАН МОНИТОР
         POP   PSW           ; ВОССТ-ТЬ ЗНАЧ.РЕГ.
         POP   D
         INX   H             ; АДРЕС СЛЕД. КОМАНДЫ
         XTHL
 ; СОХРАНИТЬ СОСТОЯНИЕ ПРОЦЕССОРА НА СТЕКЕ
 ; ПОЛЬЗОВАТЕЛЯ
         PUSH  H
         PUSH  D
         PUSH  B
         PUSH  PSW
       LXI   H,10
         DAD   SP            ; СТЕК ПОЛЬЗОВАТЕЛЯ
         XTHL
 ; ЗП.СОСТ.ПРОЦЕССОРА В СПЕЦ. ОБЛАСТИ ОЗУ
       LXI   D,BASETOS+8
         MVI   B,4
         XCHG
 RST0:
         DCX   H
         MOV   M,D
         DCX   H
         MOV   M,E
         POP   D
         DCR   B
       JNZ   RST0
         POP   B             ; <BC> - PC
         DCX   B             ; PC-1
         SPHL                ; СТЕК МОНИТОРА
 ; ОПРЕДЕЛИТЬ ИСТОЧНИК ПРЕРЫВАНИЯ
 ; ЕСЛИ ПРЕРЫВАНИЕ ВНЕШНЕЕ, PC=PC+1
       LXI   H,BASETOS+TLOC-TOS  ; 1 Т.ПР.
         MOV   A,M
         SUB   C
         INX   H
         MOV   A,M
         SBB   B
       JZ    RST1          ; ПРЕР-Е ПРОГРАМ.
         INX   H
         INX   H
         MOV   A,M
         SUB   C
         INX   H
         MOV   A,M
         SBB   B
       JZ    $+4
         INX   B             ; ПРЕРЫВАНИЕ ВНЕШНЕЕ
 RST1:
 ; ЗАПОМНИТЬ В ОЗУ HL И PC
       LXI   H,BASETOS+LLOC-TOS
         MOV   M,E
         INX   H
         MOV   M,D
       LXI   H,BASETOS+PLLOC-TOS
         MOV   M,C
         INX   H
         MOV   M,B
       CALL  COADR         ; АДРЕС НА ИНДИК.
 ; ВОССТАНОВЛЕНИЕ ИСХОДНЫХ ЗНАЧЕНИЙ ЯЧЕЕК
 ; В ТОЧКАХ ПРЕРЫВАНИЯ
       LXI   H,BASETOS+TLOC-TOS
         MVI   D,2
 RST2:
         MOV   C,M
         XRA   A
         MOV   M,A
         INX   H
         MOV   B,M
         MOV   M,A
         INX   H
         MOV   A,C
         ORA   B
       JZ    RST3          ; НЕТ ТОЧКИ ПР-Я
         MOV   A,M
         STAX  B
 RST3:
         INX   H
         DCR   D
       JNZ   RST2
       JMP   START
 CIADR:
 ; ПРИЕМ N ПАРАМЕТРОВ, N = <C>
         MOV   B,C
 CIADLP:
       CALL  ERSIND
         MVI   C,TYPEAD
       CALL  PARAM
         XTHL
         PUSH  H
         DCR   B
       JNZ   CIADLP
         RET
 COMPA:
 ; ЕСЛИ <HL>+1 = 0 ИЛИ > DE, CARRY=1
         INX   H
         MOV   A,L
         ORA   H
         STC
         RZ
 COMPA1:
         MOV   A,E
         SUB   L
         MOV   A,D
         SBB   H
         RET
 PARAM:
 ; ПРИЕМ С КОНСОЛИ HEX-ПАРАМЕТРА: BYTE ИЛИ
 ; ADDRESS. ПРЕОБР-Е В ДВОИЧНОЕ ПРЕДСТАВЛ-Е
 ; ВХ:<C>-ТИП ПАРАМЕТРА, 0-BYTE, 1-ADDRESS
 ; НАБОР ПАРАМЕТРА ЗАКАНЧ-ТСЯ: ПРОБЕЛ, ВК
 ; ВЫХ: HL-ДВОИЧНОЕ ПРЕДСТАВЛЕНИЕ ПАРАМЕТРА
       CALL  PCHK          ; ПРОБЕЛ, ВК - ?
                             ; ЕСЛИ ДА, ZERO=1
       JZ    ERROR
 PARM1:
         PUSH  B
       LXI   H,0           ; ИСХ.ЗНАЧ.РЕЗУЛЬТ.
 PARCNT:
 ; КОД СИМВОЛА В <A>
 ; ПРОВЕРКА ПРИНЯТОГО СИМВОЛА
 ; ПРЕОБРАЗОВАНИЕ ПАРАМЕТРА
         CPI   '0'
       JC    ERROR         ; НЕ HEX-СИМВОЛ
         PUSH  H             ; ЗП. РЕЗУЛЬТАТ
         MOV   B,C
         MOV   C,A
       CALL  CO            ; ОТОБР-ТЬ СИМВОЛ
         POP   H             ; ВОССТ-ТЬ РЕЗУЛЬТАТ
 ; ПРЕОБРАЗ. КОДА ПРИНЯТОГО СИМВОЛА В
 ; ДВОИЧНОЕ ПРЕДСТАВЛЕНИЕ
         MOV   A,C
         SUI   '0'
         CPI   10
       JC    $+5
         SUI   7
 ; НАКОПЛЕНИЕ РЕЗУЛЬТАТА
         DAD   H             ; СДВИГ РЕЗУЛЬТАТА
         DAD   H             ; ВЛЕВО НА ТЕТРАДУ
         DAD   H
         DAD   H
         ORA   L
         MOV   L,A
         MOV   C,B
         PUSH  H
       CALL  PCHK          ; ПРОБЕЛ, ВК - КОНЕЦ
         POP   H
       JNZ   PARCNT
         POP   B
         RET
 PCHK:
 ; ПРОВЕРКА СИМВОЛА НА "ПРОБЕЛ" И "ВК"
 ; ЕСЛИ "ПРОБЕЛ" - ZERO=1
 ; ЕСЛИ "ВК"     - ZERO=1, CARRY=1
       CALL  CI            ; ПРИЕМ СИМВОЛА
         CPI   SPACE
         RZ
         CPI   CR
         STC
         RZ
         CMC
         RET
 ERROR:
 ; ОБРАБОТКА ОШИБОК
 ; ГАШЕНИЕ ЭКРАНА И ВЫВОД СИМВОЛА "?"
       CALL  ERSIND
       LXI   B,0D3H        ; B-N ИНД., C -"?"
       CALL  CONC
       JMP   START
 CONVBIN:
 ; ПРЕОБР-Е МЛ.ТЕТРАДЫ БАЙТА В HEX-ВИД
         ANI   0FH
         ADI   90H
         DAA
         ACI   40H
         DAA
         RET
 COBYTE:
 ; ВЫВОД НА ИНДИКАЦИЮ БАЙТА В HEX ФОРМЕ
 ; <C> БАЙТ
 ; ПРЕОБРАЗОВАНИЕ БАЙТА В HEX-ФОРМУ
         MVI   B,TYPEBT
         MOV   A,C
         RRC
         RRC
         RRC
         RRC
       CALL  CONVBIN
         PUSH  B
         MOV   C,A
       CALL  CO
         POP   B
         MOV   A,C
       CALL  CONVBIN
         MOV   C,A
       JMP   CO
 COADR:
 ; ВЫВОД НА ИНДИКАЦИЮ АДРЕСА В HEX ФОРМЕ
 ; <BC>-АДРЕС
         PUSH  B
         MOV   C,B
         MVI   B,TYPEAD
       CALL  COBYTE+2
         POP   B
         MVI   B,TYPEAD
       JMP   COBYTE+2
 CONC:
 ; ВЫВОД СИМВОЛА НА КОНСОЛЬ
 ; <B>-N ИНДИКАТОРА
 ; <C>-КОД СИМВОЛА
       LXI   H,BUFCD
         MOV   E,B
         MVI   D,0
         DAD   D
         MOV   M,C
         RET
 ERSBT:
 ; ГАШЕНИЕ ИНДИКАЦИИ ДАННЫХ
       LXI   H,ERASE
       SHLD  BUFCD
         RET
 ERSIND:
 ; ГАШЕНИЕ ИНДИКАЦИИ
       CALL  ERSBT
 ERSADR:
 ; ГАШЕНИЕ АДРЕСНОЙ ИНДИКАЦИИ
       LXI   H,ERASE
       SHLD  BUFCD+2
       SHLD  BUFCD+4
         RET
 CI:
 ; РЕГЕНЕРАЦИЯ ИЗОБРАЖЕНИЯ НА ИНДИКАТОРАХ
 ; СКАНИРОВАНИЕ КОНСОЛИ И ПРИЕМ СИМВОЛОВ
 ; ПРЕОБРАЗОВАНИЕ ПРИНЯТЫХ КОДОВ
         PUSH  B
 CIBEG:
 ; РЕГЕНЕРАЦИЯ
       LXI   H,BUFCD       ; АДР.БУФ.ВЫВОДА
         MVI   B,NMBIND      ; АДР.ИНДИК.
 CILOOP:
 ; ЦИКЛ РЕГЕНЕРАЦИИ
 ; N ИНДИКАТОРА
         MOV   A,B
         OUT   PORTA
 ; ВЫВОД ДАННЫХ НА ИНДИКАТОР
         MOV   A,M
         OUT   PORTB
 ; СКАНИРОВАНИЕ КОНСОЛИ
         IN    PORTC         ; ЧТЕНИЕ СОСТОЯНИЯ
         ANI   74H           ; МОМЕНТ НАЖАТИЯ
         CPI   74H           ; КЛАВИШИ
 ; СБРОС ИНДИКАЦИИ
         MVI   A,ERASE
         OUT   PORTB
       JNZ   CISMB         ; КЛАВИША НАЖАТА
         INX   H             ; РЕГЕНЕРИРОВАТЬ СЛЕД.
         MOV   A,B           ; ИНДИКАТОР
         RRC                 ; N СЛЕД.ИНДИКАТОРА
         MOV   B,A
       JNC   CILOOP
       JMP   CIBEG         ; СНАЧАЛА
 CISMB:
 ; ЗАДЕРЖКА 10 МС, БОРЬБА С ДРЕБЕЗГОМ
       CALL  DELAY
 ; ВВОД КОДА СИМВОЛА
         IN    PORTC
         MOV   C,A
 ; ОЖИДАНИЕ МОМЕНТА ОТПУСКАНИЯ КЛАВИШИ
         IN    PORTC
         ANI   74H
         CPI   74H
       JNZ   $-6
       CALL  DELAY
 CONV:
 ; ПРЕОБРАЗОВАНИЕ КОДА ПРИНЯТОГО СИМВОЛА
 ; В КОД ASCII. КОД ASCII ЦИФР. СИМВОЛА:
 ; (COD AND 0EFH) / 8 + АДР.ИНДИК-РА / 8
 ; КОД ASCII ФУНКЦИОНАЛЬНОГО СИМВОЛА:
 ; (COD AND 0EFH) / 16 + АДР.ИНДИК-РА - 1
         MOV   A,C
         ANI   10H
       JNZ   $+5           ; РЯД КЛАВИШ НЕ 0
         MVI   C,0
         MOV   A,C           ; CODE
         ANI   64H
         RAR
         RAR
         RAR                 ; /8
         MOV   C,A
 ; РАЗДЕЛЕНИЕ ФУНКЦ-Х И ЦИФРОВЫХ КЛАВИШ
 ; ЕСЛИ МЛ. 2 РАЗРЯДА N ИНДИКАТОРА = 0
 ; - КЛАВИША ЦИФРОВАЯ
         MOV   A,B
         ANI   3
       JNZ   FUNC
         MOV   A,B
 ; N ИНДИКАТОРА / 8
         RAR
         RAR
         RAR
         CPI   4
       JNZ   $+4
         DCR   A
 ; CODE.DGT=
         ADD   C
         ORI   '0'
         CPI   3AH
         POP   B
         RC
         ADI   7
         RET
 FUNC:
 ; CODE.FUN
         DCR   A
         MOV   B,A
         MOV   A,C
         RRC
         ADD   B
         POP   B
         RET
 CO:
 ; ПРЕОБРАЗОВАНИЕ КОДА ASCII СИМВОЛА В ЕГО
 ; ФИЗИЧЕСКОЕ ПРЕДСТАВЛЕНИЕ, ЗАПИСЬ ПОЛУ-
 ; ЧЕННОГО КОДА В БУФЕР ВЫВОДА. СДВИГ ТЕК.
 ; СОСТОЯНИЯ ИНДИКАТОРОВ НА 1 ШАГ ВЛЕВО
 ; ПОСРЕДСТВОМ МОДИФИКАЦИИ БУФЕРА ВЫВОДА,
 ; ОТОБРАЖАЮЩЕГО СОСТОЯНИЕ ИНДИКАТОРОВ.
 ; <C> - КОД СИМВОЛА ASCII
 ; <B> - ТИП ДАННЫХ, 0-BYTE, 1-ADDRESS
 ; ПРЕОБР.КОДА СИМВ.ASCII В КОД ИНДИК-РА
       LXI   H,SMBTBL
         MOV   A,C
         CPI   'A'
       JC    $+5
         SUI   7
         ANI   0FH
         MOV   E,A
         MVI   D,0
         DAD   D
         MOV   E,M           ; КОД СИМВОЛА
 ; СДВИГ ЕЛЕМЕНТОВ БУФЕРА ВЫВОДА НА 1
 ; ШАГ ВЛЕВО В АДРЕСНОЙ ИЛИ БАЙТОВОЙ
 ; ЕГО ЧАСТИ. ЗП.В ПОЛЕ МЛ.ИНДИК-РА
 ; КОДА НОВОГО СИМВОЛА
       LXI   H,BUFCD
         MVI   D,2
         MOV   A,B
         ORA   A
       JZ    RALLP
         INX   H             ; БУФЕР ДАННЫХ-ADDRES
         INX   H
         MVI   D,4           ; 4 СДВИГА
 RALLP:
 ; ЦИКЛ СДВИГА И ПЕРЕЗП.ЕЛЕМЕНТОВ БУФЕРА
         MOV   A,M
         MOV   M,E
         MOV   E,A
         INX   H
         DCR   D
       JNZ   RALLP
         RET
 DELAY:
 ; ВРЕМЕННАЯ ЗАДЕРЖКА.
 ; ВРЕМЯ ОДНОГО ЦИКЛА = 10 МИКРОСЕК.
       LXI   D,TIME
         DCX   D
         MOV   A,D
         ORA   E
       JNZ   $-3
         RET
 CTBL:
 ; ТАБЛИЦА АДРЕСОВ ПРОГРАММНЫХ МОДУЛЕЙ
 ; РЕАЛИЗУЮЩИХ ЗАДАННЫЙ НАБОР ДИРЕКТИВ
         DW    REPLM
         DW    REPLRG
         DW    GOTO
         DW    CHSUM
         DW    FILE
         DW    MOVE
 TBLRG:
 ; ТАБЛИЦА ИДЕНТИФ-В РЕГИСТРОВ ПРОЦЕССОРА
 ; И АДР. ОЗУ, ГДЕ ХРАНИТСЯ ИХ СОДЕРЖИМОЕ.
         DB    73H,39H,76H   ; PC H


         DW    BASETOS+PHLOC-TOS
         DB    73H,39H,38H   ; PC L


         DW    BASETOS+PLLOC-TOS
         DB    6DH,73H,76H   ; SP H


         DW    BASETOS+SHLOC-TOS
         DB    6DH,73H,38H   ; SP L


         DW    BASETOS+SLLOC-TOS
         DB    0,0,76H       ; H


         DW    BASETOS+HLOC-TOS
         DB    0,0,38H       ; L


         DW    BASETOS+LLOC-TOS
         DB    0,0,77H       ; A


         DW    BASETOS+ALOC-TOS
         DB    0,0,7CH       ; B


         DW    BASETOS+BLOC-TOS
         DB    0,0,39H       ; C


         DW    BASETOS+CLOC-TOS
         DB    0,0,5EH       ; D


         DW    BASETOS+DLOC-TOS
         DB    0,0,79H       ; E


         DW    BASETOS+ELOC-TOS
         DB    0,0,71H       ; F


         DW    BASETOS+FLOC-TOS
 SMBTBL:
 ; ТАБЛИЦА КОДОВ СИМВОЛОВ
         DB    3FH,6,5BH,4FH



         DB    66H,6DH,7DH,7



         DB    7FH,6FH,77H,7CH



         DB    39H,5EH,79H,71H



 TOS:
 ; ТАБЛИЦА ИСХОДНЫХ ЗНАЧЕНИЙ РЕГИСТРОВ И
 ; ПРОГРАММА ОБРАБОТКИ ПРЕРЫВАНИЙ
 ELOC:   DB    0EEH
 DLOC:   DB    0DDH
 CLOC:   DB    0CCH
 BLOC:   DB    0BBH
 SPLOC:  DW    STKPTR-18
 FLOC:   DB    0FFH
 ALOC:   DB    0AAH
 EXIT:
         POP   D
         POP   B
         POP   H
         POP   PSW
         SPHL
 HLLOC:
       LXI   H,1234H
         EI
         PUSH  PSW
         MVI   A,STEPWRD
         OUT   DBGPORT
         POP   PSW
 PCLOC:
       JMP   BOOT
 TLOC:
         DW    0
         DB    0
         DW    0
         DB    0
 USRST1: DW    0
 USRST2: DW    0
 USRST3: DW    0
 USRST4: DW    0
 USRST5: DW    0
 USRST6: DW    0
 PCEND:
 ; 2. ПРОГРАММАТОР У М К
         ORG   400H
         MVI   C,3           ; ПРИНЯТЬ ПАРАМЕТРЫ
       CALL  CIADR
         POP   B
         POP   D
         POP   H
         MVI   A,40          ; [see DISCREPANCIES #1]
         PUSH  PSW
 PRLP1:
         PUSH  H
         PUSH  B
 PRLP2:
         MOV   A,M
         STAX  B
         INX   B
       CALL  COMPA
       JNC   PRLP2
         POP   B
         POP   H
         XTHL
         MVI   A,20H
         OUT   0F8H
         MOV   A,L
         RAR
         MVI   A,37H
       JC    $+5
         MVI   A,0
         OUT   0F9H
         DCR   L
         XTHL
       JNZ   PRLP1
         POP   PSW
         PUSH  H
       LXI   H,0
         XTHL
 PRLP3:
         LDAX  B
         CMP   M
       JZ    PRLP4
         XTHL
         INX   H
         XTHL
 PRLP4:
         INX   B
       CALL  COMPA
       JNC   PRLP3
         POP   B
       CALL  COADR
       CALL  CI
       JMP   START
         END
