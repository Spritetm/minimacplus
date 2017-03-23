#Makefile for a native (SDL) tme build

TARGET:=tme
MUSASHI_GEN_SRC:=musashi/m68kopac.c musashi/m68kopdm.c musashi/m68kopnz.c
MUSASHI_OP_PREGEN_SRC:=musashi/m68kops_pre.c 
OBJ:=$(MUSASHI_GEN_SRC:%.x=%.o) $(MUSASHI_OP_PREGEN_SRC:%.x=%.o) musashi/m68kcpu.o  sdl/main.o emu.o sdl/disp.o iwm.o via.o rtc.o ncr.o  sdl/hd.o
#musashi/m68kdasm.o
CFLAGS=-Wall -I. -I./musashi -Og -ggdb `sdl2-config --cflags`
LDFLAGS=`sdl2-config --libs`

$(TARGET): $(OBJ)
	$(CC) -o $(@) $(CFLAGS) $(LDFLAGS) $^

$(MUSASHI_GEN_SRC): musashi/m68kmake
	cd musashi; ../$(^)

$(MUSASHI_OP_PREGEN_SRC): $(MUSASHI_GEN_SRC)
	$(CC) -o musashi/m68kops musashi/m68kops.c
	./musashi/m68kops > $(MUSASHI_OP_PREGEN_SRC)

musashi/m68kmake: musashi/m68kmake.c
	$(CC) -o $@ $^

clean:
	rm -f $(MUSASHI_GEN_SRC) musashi/m68kmake
	rm -f $(OBJ) $(TARGET)
