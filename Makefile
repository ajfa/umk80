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
     $(BUILD)/umkrom$(EXE) $(BUILD)/criterio2$(EXE) rom/monitor.bin

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

rom/monitor.bin: rom/monitor.lst $(BUILD)/umkrom$(EXE)
	$(BUILD)/umkrom$(EXE) rom/monitor.lst rom/monitor.bin

$(BUILD)/criterio2$(EXE): tests/monitor/criterio2.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

test: all
	$(BUILD)/cpu_suite$(EXE) tests/cpu/suites --quick
	$(BUILD)/ghosting$(EXE)
	$(BUILD)/criterio2$(EXE) rom/monitor.bin

test-exm: $(BUILD)/cpu_suite$(EXE)
	$(BUILD)/cpu_suite$(EXE) tests/cpu/suites

clean:
	$(RMDIR)
