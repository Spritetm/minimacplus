#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>

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
	if (SDL_Init( SDL_INIT_VIDEO ) < 0 ) sdlDie();
	win=SDL_CreateWindow( "TME", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
	if (win==0) sdlDie();
	surf=SDL_GetWindowSurface(win);
	drwsurf=SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_RGBA32);
}

void dispDraw(char *mem) {
	int x, y, z;
	uint32_t pixels=drwsurf->pixels;
	SDL_LockSurface(drwsurf);
	for (y=0; y<SCREEN_HEIGHT; y++) {
		for (x=0; x<SCREEN_WIDTH; x+=8) {
			for (z=1; z!=0x100; z++) {
				if (*mem&z) *pixels=0xFFFFFF; else *pixels=0;
				pixels++;
			}
			mem++;
		}
	}
	SDL_UnlockSurface(drwsurf);
	SDL_BlitSurface(drwsurf, NULL, surf, NULL);
	SDL_UpdateWindowSurface(win);
}

