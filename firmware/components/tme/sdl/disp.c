//Stuff for a host-build of TME
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "mouse.h"

const int SCREEN_WIDTH = 512; 
const int SCREEN_HEIGHT = 342;

SDL_Window* win = NULL;
SDL_Surface* surf = NULL;
SDL_Surface* drwsurf=NULL;

void sdlDie() {
	printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
	exit(0);
}


void dispInit() {
	win=SDL_CreateWindow( "TME", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
	if (win==0) sdlDie();
	surf=SDL_GetWindowSurface(win);
	drwsurf=SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
}

void sdlDispAudioInit() {
	if (SDL_Init( SDL_INIT_VIDEO|SDL_INIT_AUDIO ) < 0 ) sdlDie();
}

void handleInput() {
	static int btn=0;
	static int oldx, oldy;
	SDL_Event ev;
	while(SDL_PollEvent(&ev)) {
		if (ev.type == SDL_QUIT ) {
			exit(0);
		}
		if (ev.type==SDL_MOUSEMOTION || ev.type==SDL_MOUSEBUTTONDOWN || ev.type==SDL_MOUSEBUTTONUP) {
			int x, y;
			SDL_GetMouseState(&x, &y);
			if (ev.type==SDL_MOUSEBUTTONDOWN) btn=1;
			if (ev.type==SDL_MOUSEBUTTONUP) btn=0;
			mouseMove(x-oldx, y-oldy, btn);
			oldx=x;
			oldy=y;
		}
	}
}

void dispDraw(uint8_t *mem) {
	int x, y, z;
	SDL_LockSurface(drwsurf);
	uint32_t *pixels=(uint32_t*)drwsurf->pixels;
	for (y=0; y<SCREEN_HEIGHT; y++) {
		for (x=0; x<SCREEN_WIDTH; x+=8) {
			for (z=0x80; z!=0; z>>=1) {
				if (*mem&z) *pixels=0xFF000000; else *pixels=0xFFFFFFFF;
				pixels++;
			}
			mem++;
		}
	}
	SDL_UnlockSurface(drwsurf);
	SDL_BlitSurface(drwsurf, NULL, surf, NULL);
	SDL_UpdateWindowSurface(win);
	
	//Also handle mouse here.
	handleInput();
}

