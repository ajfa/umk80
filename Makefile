# Makefile — construcción sin dependencias más allá de un compilador C11.
#
#   make            compila todo en build/
#   make test       compila y pasa las suites rápidas del 8080
#   make test-exm   pasa además 8080EXM (tarda varios minutos)
#   make clean
#
# Funciona con GNU make (Linux/macOS/MSYS2) y con mingw32-make en Windows.
# Existe también un CMakeLists.txt equivalente para quien prefiera CMake.

CC      ?= gcc
CSTD    := -std=c11
WARN    := -Wall -Wextra -Wshadow -Wconversion -Wpedantic
OPT     ?= -O2
CFLAGS  += $(CSTD) $(WARN) $(OPT) -Icore/include
BUILD   := build

ifeq ($(OS),Windows_NT)
  EXE := .exe
  MKDIR = @if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
  RMDIR = @if exist "$(subst /,\,$(BUILD))" rmdir /s /q "$(subst /,\,$(BUILD))"
else
  EXE :=
  MKDIR = @mkdir -p $(1)
  RMDIR = @rm -rf $(BUILD)
endif

CORE_SRC := core/src/i8080.c core/src/machine.c
CORE_OBJ := $(patsubst %.c,$(BUILD)/%.o,$(CORE_SRC))

.PHONY: all test test-exm clean

all: $(BUILD)/cpu_suite$(EXE) $(BUILD)/ghosting$(EXE) \
     $(BUILD)/umkrom$(EXE) $(BUILD)/umkasm$(EXE) $(BUILD)/umkdis$(EXE) \
     $(BUILD)/criterio2$(EXE) $(BUILD)/criterio4$(EXE) \
     rom/monitor.bin $(BUILD)/umk80$(EXE)

# El frontend hace aritmética de píxeles a mansalva; -Wconversion ahí sólo
# genera ruido. El núcleo sí se compila con él.
FEFLAGS := $(CSTD) -Wall -Wextra -Wshadow $(OPT) -Icore/include -Ifrontend

ifeq ($(OS),Windows_NT)
  PLATFORM_SRC := frontend/platform_win32.c
  PLATFORM_LIB := -lgdi32 -luser32
else
  PLATFORM_SRC := frontend/platform_sdl2.c
  PLATFORM_LIB := $(shell sdl2-config --libs 2>/dev/null || echo -lSDL2)
  FEFLAGS      += $(shell sdl2-config --cflags 2>/dev/null)
endif

$(BUILD)/umk80$(EXE): frontend/main.c frontend/panel.c $(PLATFORM_SRC) $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(FEFLAGS) $^ -o $@ $(PLATFORM_LIB)

.PHONY: run
run: $(BUILD)/umk80$(EXE) rom/monitor.bin
	$(BUILD)/umk80$(EXE) --rom rom/monitor.bin

$(BUILD)/%.o: %.c
	$(call MKDIR,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/cpu_suite$(EXE): tests/cpu/cpu_suite.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/ghosting$(EXE): tests/display/ghosting.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/umkrom$(EXE): tools/umkrom.c
	$(call MKDIR,$(BUILD))
	$(CC) $(CSTD) $(WARN) $(OPT) $< -o $@

$(BUILD)/umkasm$(EXE): tools/umkasm.c
	$(call MKDIR,$(BUILD))
	$(CC) $(CSTD) $(WARN) $(OPT) $< -o $@

$(BUILD)/umkdis$(EXE): tools/umkdis.c $(BUILD)/core/src/i8080.o
	$(call MKDIR,$(BUILD))
	$(CC) $(CSTD) $(WARN) $(OPT) -Icore/include $< $(BUILD)/core/src/i8080.o -o $@

# Vía 1: la imagen sale de la columna OBJ del listado. De paso se extrae el
# fuente, para que las dos vías partan del mismo fichero y no se desincronicen.
rom/monitor.bin rom/monitor.asm: rom/monitor.lst $(BUILD)/umkrom$(EXE)
	$(BUILD)/umkrom$(EXE) rom/monitor.lst rom/monitor.bin --asm rom/monitor.asm

$(BUILD)/criterio2$(EXE): tests/monitor/criterio2.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/criterio4$(EXE): tests/step/criterio4.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

# Vía 2: reensamblar el fuente y exigir igualdad byte a byte (PLAN.md §4).
.PHONY: verify-rom
verify-rom: rom/monitor.asm rom/monitor.bin $(BUILD)/umkasm$(EXE)
	$(BUILD)/umkasm$(EXE) rom/monitor.asm $(BUILD)/monitor_asm.bin --verify rom/monitor.bin

test: all verify-rom
	$(BUILD)/cpu_suite$(EXE) tests/cpu/suites --quick
	$(BUILD)/ghosting$(EXE)
	$(BUILD)/criterio2$(EXE) rom/monitor.bin
	$(BUILD)/criterio4$(EXE)

test-exm: $(BUILD)/cpu_suite$(EXE)
	$(BUILD)/cpu_suite$(EXE) tests/cpu/suites

clean:
	$(RMDIR)
