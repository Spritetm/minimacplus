#ifndef HEXDUMP_H
#define HEXDUMP_H

void hexdump(void *mem, int len);
void hexdumpFrom(void *mem, int len, int addrFrom);

#endif