# to generate assembler listing with LTO, add to LDFLAGS: -Wa,-adhln=$@.listing,--listing-rhs-width=200
# for better annotations add -dA -dP
# to generate assembler source with LTO, add to LDFLAGS: -save-temps=cwd

ifdef OS
	WINDOWS = 1
	SHELL = cmd.exe
endif

subdirs := $(wildcard */)
VPATH = $(subdirs)
cpp_sources := $(wildcard *.cpp) $(wildcard $(addsuffix *.cpp,$(subdirs)))
cpp_objects := $(addprefix obj/,$(patsubst %.cpp,%.o,$(notdir $(cpp_sources))))
c_sources := $(filter-out loader.c,$(wildcard *.c) $(wildcard $(addsuffix *.c,$(subdirs))))
c_objects := $(addprefix obj/,$(patsubst %.c,%.o,$(notdir $(c_sources))))
s_sources := support/gcc8_a_support.s support/depacker_doynax.s
s_objects := $(addprefix obj/,$(patsubst %.s,%.o,$(notdir $(s_sources))))
vasm_sources := $(wildcard *.asm) $(wildcard $(addsuffix *.asm, $(subdirs)))
vasm_objects := $(addprefix obj/, $(patsubst %.asm,%.o,$(notdir $(vasm_sources))))
objects := $(cpp_objects) $(c_objects) $(s_objects) $(vasm_objects)

# https://stackoverflow.com/questions/4036191/sources-from-subdirectories-in-makefile/4038459
# http://www.microhowto.info/howto/automatically_generate_makefile_dependencies.html

program = out/a
OUT = $(program)
CC = m68k-amiga-elf-gcc
OBJCOPY = m68k-amiga-elf-objcopy
AS = m68k-amiga-elf-as
VASM = vasmm68k_mot

ifdef WINDOWS
	SDKDIR = $(abspath $(dir $(shell where $(CC)))..\m68k-amiga-elf\sys-include)
else
	SDKDIR = $(abspath $(dir $(shell which $(CC)))../m68k-amiga-elf/sys-include)
endif

EXTRA_CCFLAGS ?=
CCFLAGS   = -g -MP -MMD -m68000 -O2 -nostdlib -Wextra -Wno-unused-function -Wno-volatile-register-var -fomit-frame-pointer -fno-tree-loop-distribution -fno-builtin-strlen -flto -fwhole-program -fno-exceptions -ffunction-sections -fdata-sections $(EXTRA_CCFLAGS)
CPPFLAGS  = $(CCFLAGS) -fno-rtti -fcoroutines -fno-use-cxa-atexit
ASFLAGS   = -mcpu=68000 -g --register-prefix-optional -I$(SDKDIR)
LDFLAGS   = -Wl,--emit-relocs,--gc-sections,-Ttext=0,-Map=$(OUT).map
VASMFLAGS = -m68000 -Felf -opt-fconst -nowarn=62 -dwarf=3 -quiet -x -I. -I$(SDKDIR)

all: $(OUT).exe

loader_program = out/harrier_loader
loader_objects = obj/loader.o obj/gcc8_c_support.o obj/gcc8_a_support.o obj/depacker_doynax.o

.PHONY: loader
loader: $(loader_program).exe
	@:

$(loader_program).exe: $(loader_program).elf
	$(info Elf2Hunk $(loader_program).exe)
	@elf2hunk $(loader_program).elf $(loader_program).exe

$(loader_program).elf: $(loader_objects)
	$(info Linking $(loader_program).elf)
	@$(CC) $(CCFLAGS) $(LDFLAGS) $(loader_objects) -o $@
	@m68k-amiga-elf-objdump --disassemble --no-show-raw-ins --visualize-jumps -S $@ >$(loader_program).s

$(OUT).exe: $(OUT).elf
	$(info Elf2Hunk $(program).exe)
	@$(OBJCOPY) --rename-section .text=.text.MEMF_FAST --rename-section .rodata=.rodata.MEMF_FAST --rename-section .data=.data.MEMF_FAST --rename-section .bss=.bss.MEMF_FAST $(OUT).elf $(OUT).hunk.elf
	@elf2hunk $(OUT).hunk.elf $(OUT).exe

$(OUT).elf: $(objects)
	$(info Linking $(program).elf)
	@$(CC) $(CCFLAGS) $(LDFLAGS) $(objects) -o $@
	@m68k-amiga-elf-objdump --disassemble --no-show-raw-ins --visualize-jumps -S $@ >$(OUT).s

clean:
	$(info Cleaning...)
ifdef WINDOWS
	@del /q obj\* out\*
else
	@$(RM) obj/* out/*
endif

-include $(objects:.o=.d)

$(cpp_objects) : obj/%.o : %.cpp
	$(info Compiling $<)
	@$(CC) $(CPPFLAGS) -c -o $@ $(CURDIR)/$<

$(c_objects) : obj/%.o : %.c
	$(info Compiling $<)
	@$(CC) $(CCFLAGS) -c -o $@ $(CURDIR)/$<

obj/loader.o: loader.c
	$(info Compiling $<)
	@$(CC) $(CCFLAGS) -c -o $@ $(CURDIR)/$<

$(s_objects): obj/%.o : %.s
	$(info Assembling $<)
	@$(AS) $(ASFLAGS) --MD $(@D)/$*.d -o $@ $(CURDIR)/$<

$(vasm_objects): obj/%.o : %.asm
	$(info Assembling $<)
	@$(VASM) $(VASMFLAGS) -dependall=make -depfile $(@D)/$*.d -o $@ $(CURDIR)/$<
