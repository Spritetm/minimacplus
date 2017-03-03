
#include <stdio.h>
#include <stdlib.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.h"


static void *loadRom(char *file) {
	char *ret=malloc(TME_ROMSIZE);
	int f=open(file, O_RDONLY);
	read(f, ret, TME_ROMSIZE);
	return ret;
}

int main(int argc, char **argv) {
	void *rom=loadRom("rom.bin");
	tmeStartEmu(rom);
}
