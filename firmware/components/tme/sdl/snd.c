//Stuff for a host build of TME
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <unistd.h>



static uint8_t buf[1024];
static int wp=256, rp=0;

static int soundEna=0;

static int bufLen() {
	return (wp-rp)&1023;
}

int sndDone() {
	if (!soundEna) return 0;
	return (bufLen()<512);
}

int sndPush(uint8_t *data, int volume) {
	if (!soundEna) return 0;
	while(!sndDone()) usleep(1000);
	for (int i=0; i<370; i++) {
		int s=*data;
		s=(s-128)>>(7-volume);
		buf[wp++]=s+128;
		if (wp==1024) wp=0;
		data+=2;
	}
	return 1;
}

static void sndCb(void* userdata, Uint8* stream, int len) {
	for (int i=0; i<len; i++) {
		stream[i]=buf[rp++];
		if (rp==1024) rp=0;
	}
}

void sndInit() {
	SDL_AudioSpec want, have;
	SDL_AudioDeviceID dev;

	SDL_memset(&want, 0, sizeof(want));
	want.freq = 22000;
	want.format = AUDIO_U8;
	want.channels = 1;
	want.samples = 256;
	want.callback = sndCb;

	dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (dev == 0) {
		SDL_Log("Failed to open audio: %s", SDL_GetError());
	} else {
		SDL_PauseAudioDevice(dev, 0);
		soundEna=1;
	}
}


