#ifndef ETHERTALK_H
#define ETHERTALK_H

void ethertalk_send_probe(uint8_t dest);
int ethertalk_send_long_ddp(uint8_t *data, int size);
int ethertalk_send_short_ddp(uint8_t *data, int size, uint8_t srcnode, uint8_t dstnode);
void ethertalkInit();
void ethertalkTick();



#endif