
#ifndef SC_OBJ_H
#define SC_OBJ_H

#include "sc_gui.h"

/*宏定义：判断是否为当前节点或其子节点*/
#define IS_CHILD_OR_SELF(parent, child) \
    (child != NULL && (child->level > parent->level || child == parent))

/*  控件类型枚举 */
#define OBJECT_TYPE_DEFINITIONS                           \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_SCREEN, "SCREEN")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_CANVAS, "CANVAS")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_RECT, "RECT")           \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_SLIDER, "SLIDER")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_ARC, "ARC")             \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_SC_LED, "SC_LED")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_SWITCH, "SWITCH")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_TEXTBOX, "TEXTBOX")     \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_BUTTON, "BUTTON")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_LABEL, "LABEL")         \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_LINE_EDIT, "LINE_EDIT") \
    /*----------------基础控件分界--------*/              \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_BAES_END, "BAES_END")   \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_CHART, "CHART")         \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_IMAGE, "IMAGE")         \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_IMAGEZIP, "IMAGEZIP")   \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_IMAGE_INDEXQOI, "INDEXQOI") \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_ROTATE, "ROTATE")       \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_MENU, "MENU")           \
    OBJECT_TYPE_ENTRY(SC_OBJ_TYPE_KEYBOARD, "KEYBOARD")

/* 展开 OBJECT_TYPE_DEFINITIONS 来定义枚举  */
typedef uint8_t sc_obj_type_t;
enum
{
#define OBJECT_TYPE_ENTRY(id, name) id,
    OBJECT_TYPE_DEFINITIONS
#undef OBJECT_TYPE_ENTRY // 立即取消定义，防止污染
        SC_OBJ_TYPE_MAX, // 自动计算枚举的总数
};

/* 控件属性枚举 */
typedef uint8_t sc_obj_Attr_t;
enum
{
    SC_OBJ_ATTR_BOOL =  0x01,   // 开关控件
    SC_OBJ_ATTR_FOCUS = 0x02,   // 可聚焦
    SC_OBJ_ATTR_VIRTUAL = 0x04, // 虚拟控件
    SC_OBJ_ATTR_HIDDEN = 0x08,  // 隐藏控件
};
/* 控件标志枚举 */
typedef uint8_t sc_obj_flag_t;
enum
{
    SC_OBJ_FLAG_ACTIVE = 0x01,  // 活动标志
    SC_OBJ_FLAG_PRESSED = 0x02, // 按下标志
    SC_OBJ_FLAG_BOOL = 0x04,    // 开关标志
    SC_OBJ_FLAG_FOCUS = 0x08,   // 聚焦标志
};

/* 控件结构体 */
typedef struct sc_obj_t
{
    struct sc_obj_t *next;                                     // 指向下一个控件的指针
    struct sc_obj_t *parent;                                   // 父控件
    bool (*handle_event)(struct sc_obj_t *obj, sc_event_t *e); // 系统事件
    void (*sc_user_cb)(struct sc_obj_t *obj, sc_event_t *e);   // 用户回调
    sc_rect_t rect;                                            // 矩形区域
    uint8_t type : 5;                                          // 类型
    uint8_t level : 3;                                         // 层级
    uint8_t attr;                                              // 属性
    uint8_t flag;                                              // 状态
    uint8_t alpha;                                             // 透明度
} sc_obj_t;

// 触摸状态枚举
typedef uint8_t sc_touch_state_t;
enum
{
    SC_TOUCH_STATE_IDLE = 0x00,   // 无触摸
    SC_TOUCH_STATE_DOWN = 0x01,   // 触摸按下
    SC_TOUCH_STATE_MOVE = 0x02,   // 触摸移动
    SC_TOUCH_STATE_BOUNCE = 0x04, // 回弹动画
};
// 触摸上下文结构体
typedef struct sc_touch_ctx
{
    sc_obj_t *srceen;    // 根对象
    sc_obj_t *obj_focus; // 当前焦点对象
    void (*anim_cb)(struct sc_touch_ctx *);
    int16_t last_x;
    int16_t last_y;
    uint8_t touch_state; // 触摸状态
} sc_touch_ctx;

extern sc_touch_ctx g_touch; // 全局触摸上下文

/* 创建新节点并加入*/
sc_obj_t *sc_obj_init(sc_obj_t *parent, sc_obj_t *obj, uint8_t type);

/* 控件事件分发*/
void sc_touch_event(sc_event_t *e);

/* 控件事件循环*/
void sc_touch_loop(void);

/*打印控件树结构*/
void sc_obj_list_print(sc_obj_t *tree);

/*跳到下一棵树控件*/
static inline sc_obj_t *sc_obj_next_tree(sc_obj_t *cur)
{
    int base = cur->level;
    while (cur->next && cur->next->level > base)
    {
        cur = cur->next;
    }
    return cur->next; /* 可能 NULL，调用方判空即可 */
}
/*上一个节点控件*/
static inline sc_obj_t *sc_obj_prev(sc_obj_t *target)
{
    sc_obj_t *parent = target->parent; // 取出已知 parent
    sc_obj_t *prev = parent;
    for (sc_obj_t *p = parent->next; p && p != target; p = p->next)
    {
        if (p->level <= parent->level)
            break; // 子树段结束
        prev = p;
    }
    return prev; // target 的前一个节点
}

/* 标记控件为脏矩形*/
static inline void sc_obj_dirty(sc_obj_t *obj, sc_rect_t *last)
{
    if (obj == NULL || obj->attr & SC_OBJ_ATTR_VIRTUAL)
        return;
    if (last == NULL && (obj->flag & SC_OBJ_FLAG_ACTIVE)) // 无上次位置，且未标记过
    {
        return;
    }
    sc_rect_t *parent = (obj->parent != NULL) ? &obj->parent->rect : NULL; // 获取父节点
    if (last)
    {
        sc_rect_t rect = sc_rect_merge(&obj->rect, last); // 合并上次位置
        sc_dirty_mark(parent, &rect);
    }
    else
    {
        sc_dirty_mark(parent, &obj->rect);
    }
    obj->flag |= SC_OBJ_FLAG_ACTIVE; // 标记刷新
}

/* 隐藏控件 */
static inline void sc_obj_hidden(sc_obj_t *obj)
{
    sc_obj_t *child = obj;
    while (IS_CHILD_OR_SELF(obj, child))
    {
        child->attr |= SC_OBJ_ATTR_HIDDEN; // 标记隐藏
        sc_obj_dirty(child, NULL);         // 标记脏矩形
        child = child->next;
    }
}
/* 删除控件 */
static inline void sc_obj_del(sc_obj_t *obj)
{
    sc_obj_t *prev = sc_obj_prev(obj); // 前驱节点
    sc_obj_t *current = obj;           // 从obj开始遍历
    sc_obj_dirty(obj, NULL);           // 标记脏矩形
    while (IS_CHILD_OR_SELF(obj, current))
    {
        sc_obj_t *to_free = current;
        current = current->next;
        free(to_free); // 释放节点
    }
    if (prev)
    {
        prev->next = current;
    }
}
/* 设置控件位置*/
static inline void sc_obj_set_geometry(sc_obj_t *obj, int x, int y, int w, int h, sc_align_t align)
{
    if (obj == NULL)
        return;
    if (obj->parent != obj)
    {
        sc_parent_align(&obj->parent->rect, &x, &y, w, h, align);
    }
    obj->rect.x = x; // 相对坐标
    obj->rect.y = y;
    obj->rect.w = w;
    obj->rect.h = h;
    sc_obj_dirty(obj, NULL);
}

/* 移动控件子控件位置*/
static inline void sc_obj_move_srceen(sc_obj_t *obj, int move_x, int move_y)
{
    if (move_x == 0 && move_y == 0)
        return;
    sc_obj_t *child = obj->next;
    while (IS_CHILD_OR_SELF(obj, child))
    {
        sc_rect_t last = child->rect;
        child->rect.x += move_x; // 相对坐标
        child->rect.y += move_y;
        sc_obj_dirty(child, &last); // 标记脏矩形
        child = child->next;
    }
}

#endif
