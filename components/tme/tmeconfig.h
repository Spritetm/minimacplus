
#include <sdkconfig.h>

#define TME_ROMSIZE (128*1024)

#ifdef CONFIG_SPIRAM_SUPPORT_MALLOC

#if 1
//Emulate an 4MiB MacPlus
#define TME_CACHESIZE (96*1024)
#define TME_RAMSIZE (4*1024*1024)
#define TME_SCREENBUF 0x3FA700
#define TME_SCREENBUF_ALT 0x3F2700

#else
//Emulate a 512K MacPlus
#define TME_RAMSIZE (512*1024)
#define TME_SCREENBUF 0x7A700
#define TME_SCREENBUF_ALT 0x72700

#endif

#else


#ifdef CONFIG_SPIRAM_USE_MEMMAP

#define TME_RAMSIZE (4*1024*1024)
#define TME_SCREENBUF 0x3FA700
#define TME_SCREENBUF_ALT 0x3F2700

#else

//Emulate a 128KiB MacPlus/Mac128K hybrid
#define TME_CACHESIZE 0
#define TME_RAMSIZE (128*1024)
#define TME_SCREENBUF 0x1A700
#define TME_SCREENBUF_ALT 0x12700
#endif


#endif