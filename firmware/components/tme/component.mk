#
# Component Makefile
#

COMPONENT_ADD_INCLUDEDIRS := . ./musashi
COMPONENT_SRCDIRS := . musashi 

MUSASHI_GEN_SRC := musashi/m68kops_pre.c musashi/m68kopac.c musashi/m68kopdm.c musashi/m68kopnz.c
MUSASHI_GEN_OBJ := $(MUSASHI_GEN_SRC:%.c=%.o)
COMPONENT_OBJS := musashi/m68kops_pre.o musashi/m68kopac.o musashi/m68kopdm-iram.o musashi/m68kopnz.o musashi/m68kcpu-iram.o emu.o \
			iwm.o via.o rtc.o ncr.o scc.o mouse.o

#nothing in iram: 1240000
#ac nz in iram: 19%
#dm nz in iram: ng
#ac dm in iram: 23%
#dm in iram: 1709000
#cpu in iram: 1278000

#-O3:
#nothing: 13%
#dm in iram: 23%
#ac in iram: 19%
#nz in iram: 14%
#dm/ac ac/nz: ng

CFLAGS += -Wno-error=implicit-function-declaration

#Some deeper Make magic to generate the file that generates bits and pieces of the
#68K emu, and run that to get the C files we need.
#We generate the things in the src dir, which is less optimal, but eh, it works.

COMPONENT_EXTRA_CLEAN := $(addprefix $(COMPONENT_PATH)/musashi/,$(MUSASHI_GEN_SRC) m68kmake) 

#musashi/m68kops_pre.o: CFLAGS += -O3
#musashi/m68kopac.o: CFLAGS += -O3
musashi/m68kopdm.o: CFLAGS += -O3
#musashi/m68kopnz.o: CFLAGS += -O3
musashi/m68kcpu.o: CFLAGS += -O3

emu.o: CFLAGS += -O3

define makeiram
$(1): $(2)
	$$(summary) OBJCOPY $(2)
	$$(OBJCOPY) --rename-section .text=.iram1 --rename-section .literal=.iram1 $(2) $(1)

$(2): $(COMPONENT_PATH)/$(2:%.o=%.c)
	$$(summary) CC $$@
	$$(CC) $$(CFLAGS:-ffunction-sections=) $$(CPPFLAGS) -O3 $$(addprefix -I ,$$(COMPONENT_INCLUDES)) $$(addprefix -I ,$$(COMPONENT_EXTRA_INCLUDES))  -c $$< -o $$@
endef

$(foreach obj, $(MUSASHI_GEN_OBJ) musashi/m68kcpu.o, $(eval $(call makeiram, $(obj:%.o=%-iram.o), $(obj))))


#Using wildcard magic here because otherwise Make will invoke this multiple times.
#$(COMPONENT_PATH)/musashi/
$(COMPONENT_PATH)/musashi/m68kop%.c: $(COMPONENT_PATH)/musashi/m68kmake
	cd $(COMPONENT_PATH)/musashi/; ./m68kmake

$(COMPONENT_PATH)/musashi/m68kops_pre.c: $(COMPONENT_PATH)/musashi/m68kops_gen
	cd $(COMPONENT_PATH)/musashi/; ./m68kops_gen > m68kops_pre.c

$(COMPONENT_PATH)/musashi/m68kops_gen: $(COMPONENT_PATH)/musashi/m68kops.c
	cd $(COMPONENT_PATH)/musashi/; $(HOSTCC) -std=gnu99 -o m68kops_gen m68kops.c

$(COMPONENT_PATH)/musashi/m68kmake: $(COMPONENT_PATH)/musashi/m68kmake.c
	$(HOSTCC) -std=gnu99 -o $@ $^
