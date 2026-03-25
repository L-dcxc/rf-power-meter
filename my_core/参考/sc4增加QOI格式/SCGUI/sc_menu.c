
#include "sc_menu.h"
#include "sc_gui.h"

void SetLanguage_cb(void *arg);
void SetBrightness_cb(void *arg);
/// 常量多语言菜单通过英文字符串匹配进行菜单跳转
/// 注意不要拼错或使用重复的title
static const Menu_t home_menu[] =
    {
        {
            .title = "Exit",
            .parent = NULL,
            .items = (const LangItem[]){
                {"home", "home"}, // 按确定进入菜单
            },
            .items_cnt = 0, // 为0不显示
            .items_cb = NULL,
        },
        {
            .title = "home",  // 主菜单
            .parent = "Exit", // 返回到上级
            .items = (const LangItem[]){
                {"Settings", "设置"},
                {"Help", "帮助"},
                {"Exit", "退出 "},
            },
            .items_cnt = 3,
            .items_cb = NULL,
        },
        {
            .title = "Settings", // 设置菜单
            .parent = "home",    // 返回到上级
            .items = (const LangItem[]){
                {"SetAudio", "音量设置 "},
                {"SetBrightness", "亮度 "},
                {"SetLanguage", "语言设置 "},
                {"Break", "返回 "},
            },
            .items_cnt = 4,
            .items_cb = NULL,
        },
        {
            .title = "Help",  // 帮助菜单
            .parent = "home", // 返回到上级
            .items = (const LangItem[]){
                {"Version", "版本号 "},
                {"about", "关于 "},
                {"Break", "返回 "},
            },
            .items_cnt = 3,
            .items_cb = NULL,
        },
        {
            .title = "SetLanguage", // 帮助菜单
            .parent = "Settings",   // 返回到上级
            .items = (const LangItem[]){
                {"English", "英文 "},
                {"Chinese", "中文 "},
                {"Break", "返回 "},
            },
            .items_cnt = 3,
            .items_cb = SetLanguage_cb,
        },
        {
            .title = "SetBrightness", // 亮度设置
            .parent = "Settings",     // 返回到上级
            .items = (const LangItem[]){
                {"1", "低 "},
                {"2", "中 "},
                {"3", "高 "},
                {"Break", "返回 "},
            },
            .items_cnt = 4,
            .items_cb = SetBrightness_cb,
        },
        {
            .title = "Version", // 版本号
            .parent = "Help",   // 返回到上级
            .items = (const LangItem[]){
                {"V3.0", "V3.0"},
                {"Break", "返回 "},
            },
            .items_cnt = 2,
            .items_cb = NULL,
        },
        {
            .title = "about", // 关于
            .parent = "Help", // 返回到上级
            .items = (const LangItem[]){
                {"menu demo", "菜单例程 "},
                {"Break", "返回 "},
            },
            .items_cnt = 2,
            .items_cb = NULL,
        },
};

/// 通过字符查找对应的菜单
static Menu_t *find_menu(const char *title)
{
    if (title == NULL)
        return NULL;
    for (int i = 0; i < sizeof(home_menu) / sizeof(home_menu[0]); i++)
    {
        if (sc_strcmp(home_menu[i].title, title) == 0)
        {
            return (Menu_t *)&home_menu[i]; // 找到匹配的菜单
        }
    }
    return NULL; // 未找到匹配的菜单
}

/// 菜单条目
const char *sc_menu_items(sc_menu_t *p, int indx)
{
    if (p->lange == 0)
    {
        return p->head->items[indx].en;
    }
    else if (p->lange == 1)
    {
        return p->head->items[indx].ch;
    }
    return NULL;
}
/// 返回上级
void sc_menu_parent(sc_menu_t *p)
{
    Menu_t *parent = find_menu(p->head->parent);
    if (parent)
    {
        p->head = parent;
        if (p->level > 0)
            p->level--;
        // printf("[Menu] 返回上一级 %s\n", p->head->title);
    }
}
/// 进入子菜单
void sc_menu_child(sc_menu_t *p)
{
    uint8_t cursor = p->cursor[p->level];                 // 记录每一级
    Menu_t *child = find_menu(p->head->items[cursor].en); // 用英文查找
    if (child)
    {
        p->head = child;
        if (p->level < 4)
        {
            p->level++;
            p->cursor[p->level] = 0;
        }
        // printf("[Menu] 进入菜单: %s\n",p->head->title);
    }
    else
    {
        if (sc_strcmp("Break", p->head->items[cursor].en) == 0)
        {
            sc_menu_parent(p);
            return;
        }
        if (p->head->items_cb) //
        {
            // printf("[Menu] 执行操作\n");
            p->head->items_cb(p);
        }
    }
}
// 组合选框与文字
void sc_menu_items_draw(sc_pfb_t *dest, sc_menu_t *p, int indx)
{
    sc_rect_t coord = p->rect[indx];
    coord.x += p->offs_x;
    coord.y += p->offs_y;
    if (indx < p->head->items_cnt)
    {
        uint16_t bc = (p->cursor[p->level] == indx) ? gui->bc : gui->bkc;                // 选中的条目
        sc_button_t btn;
        sc_init_button(&btn , gui->font,sc_menu_items(p, indx), p->tc, ALIGN_CENTER);
        sc_draw_button(dest, &coord, &btn,bc, bc, 4,4,255);
    }
    else
    {
        sc_draw_Fill(dest, coord.x, coord.y, coord.w, coord. h, gui->bkc,255);
    }
}
/// 操作光标
void sc_menu_loop_key(sc_menu_t *p, int cmd)
{
    int cursor_cnt = -1;
    uint8_t *cursor = &p->cursor[p->level]; // 记录每一级的光标
    if (cmd == CMD_DOWN)
    {
        cursor_cnt = p->head->items_cnt;
        if (cursor_cnt > 0)
        {
            *cursor = (*cursor + 1) % cursor_cnt;
            // printf("[Menu] 下移，当前选中: %s\n", sc_menu_items(p,*cursor));
        }
    }
    else if (cmd == CMD_UP)
    {
        cursor_cnt = p->head->items_cnt;
        if (cursor_cnt > 0)
        {
            *cursor = (*cursor - 1 + cursor_cnt) % cursor_cnt;
            // printf("[Menu] 上移，当前选中: %s\n", sc_menu_items(p,*cursor));
        }
    }
    else if (cmd == CMD_BACK)
    {
        cursor_cnt = p->head->items_cnt;
        sc_menu_parent(p);
    }
    else if (cmd == CMD_ENTER)
    {
        // printf("[Menu] 执行操作\n");
        cursor_cnt = p->head->items_cnt;
        sc_menu_child(p);
    }
    if (cursor_cnt != -1) // 有事件
    {
        int draw_cnt = SC_MAX(p->head->items_cnt, cursor_cnt); // 取最大值
        if (draw_cnt)
        {
            for (int i = 0; i < draw_cnt; i++)
            {
                sc_menu_items_draw(NULL, p, i);
            }
        }
    }
}

void SetLanguage_cb(void *arg)
{
    sc_menu_t *p = (sc_menu_t *)arg;
    uint8_t cursor = p->cursor[p->level]; // 当前光标
    p->lange = cursor;
}

void SetBrightness_cb(void *arg)
{
    // sc_menu_t *p=(sc_menu_t*)arg;
    // uint8_t cursor=p->cursor[p->level];               //当前光标
    // bsp_set_pwm(cursor);
}
/// 初始化
void sc_menu_init(sc_menu_t *p, int xs, int ys, int w, int h)
{
    printf("[Menu] 按回车进入菜单 \n");
    p->head = home_menu;
    p->cursor[0] = 0;
    p->lange = 0;
    p->level = 0;
    p->offs_x=0;
    p->offs_y=0;
    p->tc= C_RED;
    //=====初始化布局为列表或9宫========
    for (int i = 0; i < sizeof(p->rect) / sizeof(p->rect[0]); i++)
    {
        sc_rect_t *rect = &p->rect[i];
        rect->x = xs;
        rect->y = ys + i * (h + 10);
        rect->w = w;
        rect->h = h;
        rect++;
    }
}

