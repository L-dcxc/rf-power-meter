
#include "sc_widget.h"

// 1. Canvas容器子控件循环逻辑
static inline void sc_canvas_cycle(sc_obj_t *child, int16_t virtual_w, int16_t virtual_h)
{
    if (child->rect.x + child->rect.w <= 0)
    {
        child->rect.x += virtual_w;
    }
    else if (child->rect.x + child->rect.w > virtual_w)
    {
        child->rect.x -= virtual_w;
    }
    if (child->rect.y + child->rect.h <= 0)
    {
        child->rect.y += virtual_h;
    }
    else if (child->rect.y + child->rect.h > virtual_h)
    {
        child->rect.y -= virtual_h;
    }
}

// 2. Canvas容器子控件移动逻辑
static void sc_obj_move_Canvas(sc_obj_t *obj, int16_t move_x, int16_t move_y)
{
    sc_rect_t last = obj->rect;
    sc_obj_t *child = obj->next;
    int16_t virtual_w = 0;
    int16_t virtual_h = 0;
    if (obj->type == SC_OBJ_TYPE_CANVAS)
    {
        Canvas_t *Canvas = (Canvas_t *)obj;
        if (Canvas->cycle_mode >= CANVAS_MOVE_CYCLE) // 判断是否需要循环
        {
            virtual_w = Canvas->virtual_w + Canvas->base.rect.w;
            virtual_h = Canvas->virtual_h + Canvas->base.rect.h;
        }
        Canvas->view_x += move_x;
        Canvas->view_y += move_y;
    }
    else
    {
        obj->rect.x += move_x; // 相对坐标
        obj->rect.y += move_y;
        sc_obj_dirty(obj, &last);
    }
    while (child != NULL && child->level > obj->level)
    {
        sc_rect_t last = child->rect;
        child->rect.x += move_x; // 相对坐标
        child->rect.y += move_y;
        sc_obj_dirty(child, &last);
        if (virtual_w != 0 || virtual_w != 0)
        {
            sc_canvas_cycle(child, virtual_w, virtual_h);
        }
        child = child->next;
    }
}

static u32 last_time = 0;
// 3. Canvas容器回弹逻辑
static void Canvas_bounce_cb(sc_touch_ctx *ctx)
{
    Canvas_t *Canvas = (Canvas_t *)ctx->obj_focus;
    if (Canvas == NULL)
        return; 
    uint32_t t=   (system_tick- last_time);
		if(t==0) return;
	last_time= system_tick;
    int x = t/2;
    int y = x;
    int errx = Canvas->mov_x - Canvas->view_x;
    int erry = Canvas->mov_y - Canvas->view_y;
    if (erry != 0 || errx != 0)
    {
        int16_t delta_x = (SC_ABS(errx) < x) ? (errx % x) : (errx < 0 ? -x : x);
        int16_t delta_y = (SC_ABS(erry) < y) ? (erry % y) : (erry < 0 ? -y : y);
        sc_obj_move_Canvas(ctx->obj_focus, delta_x, delta_y);
        g_touch.touch_state |= SC_TOUCH_STATE_BOUNCE;
    }
    else
    {
        g_touch.touch_state = SC_TOUCH_STATE_IDLE; // 回弹
        ctx->anim_cb = NULL;                       // 回弹结束
    }
}

// 4. Canvas容器cmd事件
static bool Canvas_cmd_event(sc_obj_t *obj, sc_event_t *event)
{
    Canvas_t *Canvas = (Canvas_t *)obj;
    if(g_touch.anim_cb) return false; // 有回弹动画，不处理cmd事件
    if (event->type == SC_EVENT_TYPE_CMD)
    {

        switch (event->dat.cmd)
        {
        case CMD_UP:
        case CMD_DOWN:
            if (Canvas->cycle_mode & CANVAS_MOVE_Y)
            {
                Canvas->mov_y = (event->dat.cmd == CMD_UP) ? obj->rect.h : -obj->rect.h; // 上页或下页坐标
                Canvas->mov_y += Canvas->view_y;                                           // 记录当前坐标
                g_touch.anim_cb = Canvas_bounce_cb;
                last_time= system_tick;
            }
            break;
        case CMD_LEFT:
        case CMD_RIGHT:
            if (Canvas->cycle_mode & CANVAS_MOVE_X)
            {
                Canvas->mov_x = (event->dat.cmd == CMD_LEFT) ? obj->rect.w : -obj->rect.w; // 左页或右页坐标
                Canvas->mov_x += Canvas->view_x;
                g_touch.anim_cb = Canvas_bounce_cb;
                last_time= system_tick;
            }
            break;
        }
        return true;
    }
    return false;
}

// 5. Canvas容器触摸事件处理
static bool handle_event_canvas(sc_obj_t *obj, sc_event_t *event)
{
    Canvas_t *Canvas = (Canvas_t *)obj;
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest=event->dat.arg;
       // sc_draw_Frame(dest, obj->rect.x, obj->rect.y, obj->rect.w, obj->rect.h, 1, C_BLUE, obj->alpha); // 绘制矩形
        return true;
    }
    switch (event->type)
    {
    case SC_EVENT_TOUCH_DOWN:
        Canvas->mov_x = Canvas->view_x;
        Canvas->mov_y = Canvas->view_y; // 记录当前坐标
        return true;                     // 聚焦当前控件
    case SC_EVENT_TOUCH_MOVE:
    {
        int16_t delta_x = 0;
        int16_t delta_y = 0;
        if (Canvas->cycle_mode & CANVAS_MOVE_X)
        {
            delta_x = event->dat.pos[0] - g_touch.last_x; // 计算触摸增量
            if ((delta_x > 0 && Canvas->view_x > 0) || (delta_x < 0 && Canvas->view_x < -Canvas->virtual_w))
            {
                if ((Canvas->cycle_mode & CANVAS_MOVE_CYCLE) == 0)
                    delta_x /= 8; // 边界阻力
            }
        }
        if (Canvas->cycle_mode & CANVAS_MOVE_Y)
        {
            delta_y = event->dat.pos[1] - g_touch.last_y; // 计算触摸增量
            if ((delta_y > 0 && Canvas->view_y > 0) || (delta_y < 0 && Canvas->view_y < -Canvas->virtual_h))
            {
                if ((Canvas->cycle_mode & CANVAS_MOVE_CYCLE) == 0)
                    delta_y /= 8; // 边界阻力
            }
        }
        if (delta_x != 0 || delta_y != 0)
        {
            sc_obj_move_Canvas(obj, delta_x, delta_y);
        }
    }
    break;
    case SC_EVENT_TOUCH_UP:
        if (g_touch.touch_state & SC_TOUCH_STATE_MOVE)
        {
            int errx = Canvas->view_x - Canvas->mov_x;
            int erry = Canvas->view_y - Canvas->mov_y;
            if (SC_ABS(errx) > obj->rect.w / 4)
            {
                Canvas->mov_x += errx > 0 ? obj->rect.w : -obj->rect.w; // 上一页或下一页坐标
            }
            if (SC_ABS(erry) > obj->rect.h / 4)
            {
                Canvas->mov_y += erry > 0 ? obj->rect.h : -obj->rect.h; // 上一页或下一页坐标
            }
            g_touch.anim_cb = Canvas_bounce_cb;
            last_time= system_tick;
        }
        break;
    default:
        return Canvas_cmd_event(obj, event);
        break;
    }
    return false;
}

/// 创建Canvas控件
sc_obj_t *sc_create_canvas(sc_obj_t *parent, int x, int y, int w, int h, int virtual_w, int virtual_h, sc_align_t align)
{
    Canvas_t *Canvas = (Canvas_t*)sc_obj_init(parent, malloc(sizeof(Canvas_t)), SC_OBJ_TYPE_CANVAS); // 初始化基类
    if (Canvas)
    {
        Canvas->base.handle_event = handle_event_canvas;            // 设置事件处理函数
        Canvas->base.attr |= SC_OBJ_ATTR_VIRTUAL|SC_OBJ_ATTR_FOCUS; // 设置属性
        Canvas->virtual_w = virtual_w - w;
        Canvas->virtual_h = virtual_h - h;
        Canvas->view_x = 0;
        Canvas->view_y = 0;
        Canvas->cycle_mode = CANVAS_MOVE_X|CANVAS_MOVE_CYCLE;          // 默认支持X和Y轴平移
        sc_obj_set_geometry(&Canvas->base, x, y, w, h, align); // 初始化基类位置大小
        return &Canvas->base;
    }
    return NULL;
}
