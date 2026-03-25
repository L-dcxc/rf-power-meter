
#include "sc_widget.h"

static sc_lpos_t lpos={0};
static int16_t edit_pos_x = 0;      // 光标位置
static int16_t edit_pos_y = 0;      // 光标位置
static Keyboard_t g_keyboard = {0}; // 静态内存，键盘只有一个

// 创建键盘对象,键盘对象为静态只有一个
static sc_obj_t *sc_create_keyboard(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 获取光标
static bool sc_get_line_edit_pos(Line_edit_t *edit, sc_lpos_t *pos, int16_t x, int16_t y, int cmd)
{
    sc_rect_t *parent = &edit->base.rect;
    return sc_get_Label_pos(parent, &edit->label, pos,  x,  y,  cmd);
}
// 绘制编辑光标
static void sc_draw_line_edit_pos(Line_edit_t *edit)
{
    edit_pos_x = lpos.x - edit->base.rect.x;
    edit_pos_y = lpos.y - edit->base.rect.y;
    sc_dirty_mark(NULL, (sc_rect_t *)&lpos);
}
// 清除编辑光标
static void sc_clear_line_edit_pos(Line_edit_t *edit)
{
    sc_rect_t last_pos = {edit->base.rect.x + edit_pos_x, edit->base.rect.y + edit_pos_y, lpos.w, lpos.h};
    sc_dirty_mark(NULL, &last_pos);
}

// 键盘输入字符
static bool sc_keyboard_line_edit(Line_edit_t *edit, int cmd)
{
    if (edit == NULL || edit->editbuf_size == 0)
        return false;
    int x = lpos.x;
    int y = lpos.y;
    char *text_buf = edit->label.text;
    char *p = &text_buf[lpos.mark]; // 光标位置
    switch (cmd)
    {
    case KB_VALUE_UP:
    case KB_VALUE_DOWN:
    case KB_VALUE_LEFT:
    case KB_VALUE_RIGHT:
        if (cmd == KB_VALUE_RIGHT)
        {
            lpos.mark++;
        }
        else if (cmd == KB_VALUE_LEFT)
        {
            if (lpos.mark <= 0)
                return false;
                lpos.mark--;
        }
        else if (cmd == KB_VALUE_UP || cmd == KB_VALUE_DOWN)
        {
            y += (cmd == KB_VALUE_DOWN) ? lpos.h : -lpos.h;
            cmd = 0; // 模拟触控
        }
        if (sc_get_line_edit_pos(edit, &lpos, x, y, cmd))
        {
            sc_clear_line_edit_pos(edit);
            sc_draw_line_edit_pos(edit);
            return true;
        }
        break;
    case KB_VALUE_BACKSPACE:
        if (lpos.mark > 0)
        {
            p--;
            do
            {
                p[0] = p[1];
                p++;
            } while (*p);
            lpos.mark--;                                  // 光标前移
            sc_get_line_edit_pos(edit, &lpos, x, y, cmd); // 右
            sc_draw_line_edit_pos(edit);
            sc_obj_dirty((sc_obj_t *)edit, NULL);
            return true;
        }
        break;
    case KB_VALUE_DEL:
        if (*p)
        {
            while (*p)
            {
                p[0] = p[1];
                p++;
            }
            sc_obj_dirty((sc_obj_t *)edit, NULL);
            return true;
        }
        break;
    default:
        if (((cmd <= '~') && (cmd >= ' ')) || cmd == KB_VALUE_NEWLINE)
        {
            uint16_t curr_len = strlen(text_buf); // 英文下字符数=字节数
            if (curr_len < edit->editbuf_size - 1)
            {
                // 光标在末尾：直接追加到最后（光标后无字符）
                if (lpos.mark >= curr_len)
                {
                    text_buf[curr_len] = cmd;
                    text_buf[curr_len + 1] = '\0';
                }
                else
                {
                    for (int i = curr_len; i > lpos.mark; i--)
                    {
                        text_buf[i] = text_buf[i - 1];
                    }
                    text_buf[lpos.mark] = cmd;
                }
                lpos.mark++;                                  // 光标后移（跟随插入的字符）
                sc_get_line_edit_pos(edit, &lpos, x, y, cmd); // 右
                sc_draw_line_edit_pos(edit);
                sc_obj_dirty((sc_obj_t *)edit, NULL);
                return true;
            }
        }
        break;
    }
    return false;
}

// 编辑框事件
static bool handle_event_line_edit(sc_obj_t *obj, sc_event_t *event)
{
    Line_edit_t *edit = (Line_edit_t *)obj;
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        dest->parent = &obj->parent->rect; // 绘制背景
        sc_draw_Label(dest, &obj->rect, &edit->label,edit->color,edit->fill);
        if(edit->focus==1)
        {
            sc_area_t out;
            sc_rect_t pos={obj->rect.x + edit_pos_x, obj->rect.y + edit_pos_y, 1, lpos.h};
            if(sc_rect_intersect_to_area(&obj->rect, &pos, &out))               // 光标在编辑框内才绘制
            {
                sc_draw_Fill(dest, out.xs, out.ys , 1,out.ye-out.ys, C_RED, 255); // 绘制光标
            }
        }
        dest->parent = NULL;
        return true;
    }
    if (event->type == SC_EVENT_TOUCH_DOWN)
    {
        edit->focus=1;
        if (g_keyboard.edit && g_keyboard.edit != edit)
        {
            g_keyboard.edit->focus=0;
            sc_clear_line_edit_pos(g_keyboard.edit);          // 清除光标
        }
        if (sc_get_line_edit_pos(edit, &lpos, event->dat.pos[0], event->dat.pos[1], 0))
        {
            sc_clear_line_edit_pos(edit); // 清除光标
            sc_draw_line_edit_pos(edit);  // 绘制光标
            return true;
        }
    }
    else if (event->type == SC_EVENT_TOUCH_UP)
    {
        if (g_keyboard.edit == NULL)
        {
            sc_obj_move_srceen(g_touch.srceen, 0, -50); // 上移屏幕，创建键盘
            sc_create_keyboard(g_touch.srceen, 0, 0, LD_CFG_SCREEN_WIDTH, LD_CFG_SCREEN_HEIGHT >> 1, ALIGN_BOTTOM);
        }
        g_keyboard.edit = edit; // 绑定编辑框
        return true;
    }
    else if (event->type == SC_EVENT_TYPE_CMD && g_keyboard.edit)
    {
        // sdl2键盘事件要转换成sc键盘事件
        // sc_keyboard_line_edit(Line_edit_t *edit, int cmd);
    }

    return false;
}

// 创建编辑框
sc_obj_t *sc_create_line_edit(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    Line_edit_t *p = (Line_edit_t *)sc_obj_init(parent, malloc(sizeof(Line_edit_t)), SC_OBJ_TYPE_LINE_EDIT); // 初始化基类
    if (p)
    {
        p->base.handle_event = handle_event_line_edit; // 设置事件处理函数
        p->base.attr |= SC_OBJ_ATTR_FOCUS;              //需要聚焦
        p->editbuf_size = 0;
        sc_init_Label(&p->label, 2, 2, gui->font, "edit", ALIGN_AUTO_BREAK | ALIGN_BORDER_VIS);
        sc_obj_set_geometry(&p->base, x, y, w, h, align); // 初始化基类位置大小
        p->color= gui->fc;
        p->fill= gui->bkc;
        return &p->base;
    }
    return NULL;
}
// 编辑框绑定文本缓存
void sc_set_edit_text(sc_obj_t *obj, char *text, uint16_t editbuf_size)
{
    if (obj == NULL)
        return;
    Line_edit_t *p = (Line_edit_t *)obj;
    p->label.text = text;
    p->editbuf_size = editbuf_size;
}

static uint32_t last_time=0;
static void keyboard_long_cb(sc_touch_ctx *ctx)
{
    Keyboard_t *p = (Keyboard_t *)ctx->obj_focus;
    if ((int)last_time-(int)system_tick<=0)
    {
        sc_keyboard_line_edit(p->edit, p->value); // 按键事件
        last_time= system_tick+50;
    }
}
// 键盘事件
static bool handle_event_keyboard(sc_obj_t *obj, sc_event_t *event)
{
    Keyboard_t *p = (Keyboard_t *)obj;
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        sc_draw_keyboard(dest,&obj->rect,&p->kb, p->border_color,p->color,p->color);
        return true;
    }
    if (event->type == SC_EVENT_TOUCH_DOWN)
    {
        for (int y = 0; y < p->kb.row_num; y++)
        {
            for (int x = 0; x < p->kb.col_num; x++)
            {
                const kbBtnInfo_t *kb = &p->kb.kb_tab[y * p->kb.col_num + x];
                sc_rect_t rect = sc_get_kb_pos(kb, obj->rect.x, obj->rect.y);
                if (sc_rect_touch_ctx(&rect, event->dat.pos[0], event->dat.pos[1]))
                {
                    sc_dirty_mark(NULL, &rect);               // 标记按键区域
                    p->kb.now = kb;
                    p->value= sc_get_kb_vol(kb, p->kb.upper); // 获取按键值
                    sc_keyboard_line_edit(p->edit, p->value); // 按键事件
                    g_touch.anim_cb = keyboard_long_cb;      
                    last_time= system_tick+200; 
                    return true;
                }
            }
        }
    }
    else if (event->type == SC_EVENT_TOUCH_UP)
    {
        g_touch.anim_cb = NULL; // 取消长按事件  
        if (p->kb.now)
        {
            if (p->kb.now->value == KB_VALUE_ENTER)
            {
                sc_clear_line_edit_pos(p->edit);          // 清除光标
                sc_obj_hidden(obj);                       // 隐藏键盘
                p->edit ->focus=0;
                p->edit = NULL;
                sc_obj_move_srceen(g_touch.srceen, 0, 50); // 恢复屏幕
            }
            else if (p->kb.now->value >= KB_VALUE_NUMBER_MODE && p->kb.now->value <= KB_VALUE_SHIFT_MODE)
            {
                sc_init_keyboard(&p->kb, p->kb.now->value & 0x0f); // 切换键盘模式
                sc_obj_dirty(obj, NULL);
            }
            else
            {
                sc_rect_t rect = sc_get_kb_pos(p->kb.now, obj->rect.x, obj->rect.y);
                sc_dirty_mark(NULL, &rect);
            }
            p->kb.now = NULL;
        }
    }
    return false;
}
// 创建键盘
static sc_obj_t *sc_create_keyboard(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    Keyboard_t *p = (Keyboard_t *)sc_obj_init(parent, &g_keyboard.base, SC_OBJ_TYPE_KEYBOARD); // 初始化基类
    if (p)
    {
        p->base.handle_event = handle_event_keyboard;     // 设置事件处理函数
        sc_obj_set_geometry(&p->base, x, y, w, h, align); // 初始化基类位置大小
        p->base.attr |= SC_OBJ_ATTR_FOCUS;                // 键盘需要聚焦
        p->color = C_WHITE;
        p->border_color = C_RGB(100, 100, 100);
        sc_init_keyboard(&p->kb, 3);
        return &p->base;
    }
    return NULL;
}




