#
# Component Makefile
#

COMPONENT_ADD_INCLUDEDIRS := . ./musashi
COMPONENT_SRCDIRS := . musashi 

MUSASHI_GEN_SRC := musashi/m68kops_pre.c musashi/m68kopac.c musashi/m68kopdm.c musashi/m68kopnz.c
MUSASHI_GEN_OBJ := $(MUSASHI_GEN_SRC:%.c=%.o)
COMPONENT_OBJS := $(MUSASHI_GEN_OBJ) musashi/m68kcpu.o emu.o iwm.o via.o rtc.o ncr.o

CFLAGS += -Wno-error=implicit-function-declaration

#Some deeper Make magic to generate the file that generates bits and pieces of the
#68K emu, and run that to get the C files we need.
#We generate the things in the src dir, which is less optimal, but eh, it works.

COMPONENT_EXTRA_CLEAN := $(addprefix $(COMPONENT_PATH)/musashi/,$(MUSASHI_GEN_SRC) m68kmake) 

#Using wildcard magic here because otherwise Make will invoke this multiple times.
#$(COMPONENT_PATH)/musashi/
musashi/m68kop%.c: $(COMPONENT_PATH)/musashi/m68kmake
	cd $(COMPONENT_PATH)/musashi/; ./m68kmake; $(HOSTCC) -o m68kops_gen m68kops_gen.c; ./m68kops_gen > m68kops_pre.c

$(COMPONENT_PATH)/musashi/m68kmake: $(COMPONENT_PATH)/musashi/m68kmake.c
	$(HOSTCC) -o $@ $^
