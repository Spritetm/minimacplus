
#include <stdio.h>
#include <stdlib.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "tmeconfig.h"


static void *loadRom(char *file) {
	int i;
	char *ret=malloc(TME_ROMSIZE);
	int f=open(file, O_RDONLY);
	i=read(f, ret, TME_ROMSIZE);
	if (i!=TME_ROMSIZE) {
		perror("reading rom");
		exit(1);
	}
	return ret;
}

int main(int argc, char **argv) {
	void *rom=loadRom("rom.bin");
	void *ram=malloc(TME_RAMSIZE);
	tmeStartEmu(ram, rom);
}
