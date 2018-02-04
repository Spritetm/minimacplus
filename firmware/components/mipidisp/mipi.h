#ifndef MIPI_H
#define MIPI_H

#include <stdint.h>

void mipiSend(uint8_t *data, int count);
void mipiSendMultiple(uint8_t **data, int *lengths, int count);
void mipiInit();
void mipiResync();

#endif