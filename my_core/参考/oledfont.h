#ifndef __OLEDFONT_H
#define __OLEDFONT_H

typedef struct {
  unsigned char Index[2];
  char Msk[32];
} typFNT_GB16;
//纯色填充测试矩形画圆图片显示菜单全动电子技术有限公司欢迎您综合测试程序版权所有

typedef struct {
  unsigned char Index[2];
  char Msk[72];
} typFNT_GB24;

typedef struct {
  unsigned char Index[2];
  char Msk[128];
} typFNT_GB32;

extern code unsigned char asc2_1206[95][12];
extern code unsigned char asc2_1608[95][16];
extern  typFNT_GB16 code tfont16[];
extern  typFNT_GB24 code tfont24[];
extern  typFNT_GB32 code tfont32[];
extern code unsigned char gImage_pic2[1520];
unsigned short givesize_GB16();
unsigned short givesize_GB24();
unsigned short givesize_GB32();







#endif