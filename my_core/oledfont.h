#ifndef __OLEDFONT_H
#define __OLEDFONT_H

#include "main.h"

typedef struct {
  unsigned char Index[2];
  char Msk[32];
} typFNT_GB16;

typedef struct {
  unsigned char Index[2];
  char Msk[72];
} typFNT_GB24;

typedef struct {
  unsigned char Index[2];
  char Msk[128];
} typFNT_GB32;

// ÊÊÅäSTM32£ºÒÆ³ýcode¹Ø¼ü×Ö£¬¸ÄÓÃconst
extern const unsigned char asc2_1206[95][12];
extern const unsigned char asc2_1608[95][16];
extern const typFNT_GB16 tfont16[];
extern const typFNT_GB24 tfont24[];
extern const typFNT_GB32 tfont32[];

unsigned short givesize_GB16(void);
unsigned short givesize_GB24(void);
unsigned short givesize_GB32(void);

#endif
