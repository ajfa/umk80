# Makefile — builds with no dependency beyond a C11 compiler.
#
#   make            build everything into build/
#   make test       build and run the quick acceptance checks
#   make test-exm   also run 8080EXM (takes several minutes)
#   make pack       build the portable package for this system
#   make clean
#
# Works with GNU make (Linux/macOS/MSYS2) and with mingw32-make on Windows.
# An equivalent CMakeLists.txt is provided for anyone who prefers CMake.

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
     $(BUILD)/criterion2$(EXE) $(BUILD)/criterion4$(EXE) \
     rom/monitor.bin $(BUILD)/umk80$(EXE) $(BUILD)/umkcli$(EXE)

# The frontend does pixel arithmetic everywhere; -Wconversion there is just
# noise. The core is compiled with it.
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

# One self-contained script per system, never one with a switch to pick.
.PHONY: pack
ifeq ($(OS),Windows_NT)
pack: all
	tools\pack.cmd
else
pack: all
	sh tools/pack.sh
endif

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

$(BUILD)/umkdis$(EXE): tools/umkdis.c $(BUILD)/disasm.o $(BUILD)/core/src/i8080.o
	$(call MKDIR,$(BUILD))
	$(CC) $(CSTD) $(WARN) $(OPT) -Icore/include -Itools $^ -o $@

# Path 1: the image comes from the listing's OBJ column. The source column is
# extracted at the same time, so both paths start from the same file and
# cannot drift apart.
rom/monitor.bin rom/monitor.asm: rom/monitor.lst $(BUILD)/umkrom$(EXE)
	$(BUILD)/umkrom$(EXE) rom/monitor.lst rom/monitor.bin --asm rom/monitor.asm

$(BUILD)/criterion2$(EXE): tests/monitor/criterion2.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/criterion4$(EXE): tests/step/criterion4.c $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CFLAGS) $^ -o $@

# Path 2: reassemble the source and require byte-for-byte equality (PLAN.md §4).
.PHONY: verify-rom
verify-rom: rom/monitor.asm rom/monitor.bin $(BUILD)/umkasm$(EXE)
	$(BUILD)/umkasm$(EXE) rom/monitor.asm $(BUILD)/monitor_asm.bin --verify rom/monitor.bin

$(BUILD)/disasm.o: tools/disasm.c tools/disasm.h
	$(call MKDIR,$(BUILD))
	$(CC) $(CSTD) $(WARN) $(OPT) -Icore/include -Itools -c tools/disasm.c -o $@

$(BUILD)/umkcli$(EXE): cli/umkcli.c $(BUILD)/disasm.o $(CORE_OBJ)
	$(call MKDIR,$(BUILD))
	$(CC) $(CSTD) -Wall -Wextra -Wshadow $(OPT) -Icore/include -Itools $^ -o $@

test: all verify-rom
	$(BUILD)/cpu_suite$(EXE) tests/cpu/suites --quick
	$(BUILD)/ghosting$(EXE)
	$(BUILD)/criterion2$(EXE) rom/monitor.bin
	$(BUILD)/criterion4$(EXE)
	$(BUILD)/umkcli$(EXE) --rom rom/monitor.bin --script tests/cli/smoke.txt

test-exm: $(BUILD)/cpu_suite$(EXE)
	$(BUILD)/cpu_suite$(EXE) tests/cpu/suites

clean:
	$(RMDIR)
