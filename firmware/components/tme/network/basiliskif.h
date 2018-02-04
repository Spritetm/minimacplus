#ifndef BASILISKIF_H
#define BASILISKIF_H

#include <stdint.h>

int basiliskIfInit();
int basiliskIfRecv(uint8_t *buf, int len);
int basiliskIfSend(uint8_t *buf, int len);


#endif