#ifndef DDP_H
#define DDP_H

#define DDP_SHORT_HDR_LEN 5
#define DDP_LONG_HDR_LEN 13

int ddp_get_dest_node(void *ddp);
int ddp_get_src_node(void *ddp);
int ddp_get_src_net(void *ddp);
int ddp_short_to_long(void *ddp_short, void *ddp_long, int buflen, uint8_t dest, int destnetno, uint8_t src, int srcnetno);
int ddp_long_to_short(void *ddp_long, void *ddp_short, int buflen);

int ddp_print(void *ddp, int len, int isLong);

#endif