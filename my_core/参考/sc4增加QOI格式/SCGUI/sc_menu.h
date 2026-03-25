#ifndef SC_MENU_H
#define SC_MENU_H

#include "sc_common.h"

typedef struct LangItem
{
    const char *en;  // 英文
    const char *ch;  // 中文
} LangItem;

//菜单项结构体
typedef struct
{
    const char  *title;         //标题
    const char  *parent;
    const LangItem  *items;     //菜单项数组
    uint8_t      items_cnt;     //菜单数量
    void  (*items_cb)(void *arg);
} Menu_t;

typedef struct
{
    const Menu_t  *head;
    uint8_t    cursor[5];
    uint8_t    level;
    uint8_t    lange;
    sc_rect_t  rect[5];   //最大5条
    int16_t    offs_x;
    int16_t    offs_y;
    uint16_t   tc;
} sc_menu_t;

///字符串匹配函数
static inline int sc_strcmp(const char * s1, const char * s2)
{
    while(*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

const char* sc_menu_items(sc_menu_t *p,int indx);

void sc_menu_parent(sc_menu_t *p);

void sc_menu_child(sc_menu_t *p);

void sc_menu_items_draw(sc_pfb_t *dest,sc_menu_t *p,int indx);

void sc_menu_loop_key(sc_menu_t *p,int cmd);

void sc_menu_init(sc_menu_t *p, int xs, int ys, int w, int h);
#endif  // MENU_H
