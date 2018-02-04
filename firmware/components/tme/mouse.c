/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include "mouse.h"

typedef struct {
	int dx, dy;
	int btn;
	int rpx, rpy;
} Mouse;

static const int quad[4]={0x0, 0x1, 0x3, 0x2};

Mouse mouse;

#define MAXCDX 50

void mouseMove(int dx, int dy, int btn) {
	mouse.dx+=dx;
	mouse.dy+=dy;
	if (mouse.dx>MAXCDX) mouse.dx=MAXCDX;
	if (mouse.dy>MAXCDX) mouse.dy=MAXCDX;
	if (mouse.dx<-MAXCDX) mouse.dx=-MAXCDX;
	if (mouse.dy<-MAXCDX) mouse.dy=-MAXCDX;
	if (btn) mouse.btn=1; else mouse.btn=0;
}

int mouseTick() {
	int ret=0;
	if (mouse.dx>0) {
		mouse.dx--;
		mouse.rpx--;
	}
	if (mouse.dx<0) {
		mouse.dx++;
		mouse.rpx++;
	}
	if (mouse.dy>0) {
		mouse.dy--;
		mouse.rpy++;
	}
	if (mouse.dy<0) {
		mouse.dy++;
		mouse.rpy--;
	}
	ret=quad[mouse.rpx&3];
	ret|=quad[mouse.rpy&3]<<2;
	ret|=mouse.btn<<4;
//	printf("dx %d dy %d ret %x\n", mouse.dx, mouse.dy, ret);
	return ret;
}
