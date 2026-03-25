
#include "sc_obj.h"

sc_touch_ctx g_touch = {
    .srceen = NULL,
    .obj_focus = NULL,
    .anim_cb = NULL,
    .last_x = 0,
    .last_y = 0,
    .touch_state = 0,
};

// 物体类型名称数组
const char *obj_name[] = {
    #define OBJECT_TYPE_ENTRY(id, name) [id] = name,
        OBJECT_TYPE_DEFINITIONS
    #undef OBJECT_TYPE_ENTRY // 立即取消定义
    };
/// 单向链表排序按level排序，相当于深度优先树链表
static int sc_obj_list_add(sc_obj_t *parent, sc_obj_t *obj)
{
    if (obj == NULL)
        return 0;
    sc_obj_t *prev = parent;  // 初始化前驱为parent
    sc_obj_t *tail = obj;     // 子树尾部
    sc_obj_t *child = parent; // 从parent开始遍历
    while (IS_CHILD_OR_SELF(parent, child))
    {
        if (child == obj)
        {
            return 0; // 检查是否已存在相同的节点，
        }
        prev = child; // 保存前驱
        child = child->next;
    }
    ///---------如果为子树-------
    child = obj->next;
    while (child != NULL && child->level > obj->level)
    {
        // 整个子树(A4->B5->C6)重新挂到前驱节点(root0)后面,offse=3  (root0)->(A1->B2->C3)
        child->level = child->level + (prev->level + 1) - obj->level;
        tail = child;
        child = child->next;
    }
    obj->level = parent->level + 1; // 设置新节点的level
    tail->next = prev->next;        // 连接前驱节点后面
    prev->next = obj;               // 将新节点插入到前驱节点后面
    return 1;
}

/// 初始化新节点并加入 链表
sc_obj_t *sc_obj_init(sc_obj_t *parent, sc_obj_t *obj, uint8_t type)
{
    if (obj == NULL)
    {
        return NULL; // 创建失败
    }
    obj->type = type;
    obj->flag = 0;
    obj->attr = 0;
    obj->alpha = 255;
    obj->next = NULL;
    obj->sc_user_cb = NULL;
    if (parent != NULL)
    {
        obj->parent = parent;
        sc_obj_list_add(parent, obj); // 加入到键表
    }
    else
    {
        obj->next = NULL;
        obj->level = 0;
        obj->parent = obj; // 指向自己保证(obj->parent != NULL)
    }
    return obj;
}
// 设置焦点控件
void sc_obj_set_focus(sc_obj_t *obj)
{
    if (g_touch.touch_state == SC_TOUCH_STATE_IDLE) // 如果当前焦点释放再切换
    {
        g_touch.obj_focus = obj;
    }
}
// 事件分发
void sc_touch_event(sc_event_t *e)
{
    if (g_touch.srceen == NULL)
        return;
    if (e->type == SC_EVENT_TOUCH_DOWN )
    {
        if(g_touch.touch_state!=SC_TOUCH_STATE_IDLE) return;
        sc_obj_t *focus = NULL;
        e->type = SC_EVENT_TYPE_FOCUS; // 转换为聚焦事件
        sc_obj_t *current = g_touch.srceen;
        while (IS_CHILD_OR_SELF(g_touch.srceen, current))
        {
            // 广度搜索不相交+隐藏节点跳过
            if ((current->attr & SC_OBJ_ATTR_HIDDEN) || sc_rect_touch_ctx(&current->rect, e->dat.pos[0], e->dat.pos[1]) == 0)
            {
                current = current->next;
                continue;
            }
            if (current->attr & SC_OBJ_ATTR_FOCUS || current->handle_event(current, e)) // 执行聚焦事件，返回true表示获得焦点
            {
                focus = current;
            }
            current = current->next;
        }
        g_touch.obj_focus = focus;     // 设置焦点
        e->type = SC_EVENT_TOUCH_DOWN; // 恢复事件类型
    }
    if (g_touch.obj_focus)  //有效的焦点控件，分发事件
    {
        switch (e->type)
        {
        case SC_EVENT_TOUCH_DOWN:                       // 按下事件
            g_touch.touch_state |= SC_TOUCH_STATE_DOWN; 
            g_touch.last_x = e->dat.pos[0];
            g_touch.last_y = e->dat.pos[1];
            g_touch.obj_focus->handle_event(g_touch.obj_focus, e);
            break;
        case SC_EVENT_TOUCH_MOVE:
            if (g_touch.touch_state & SC_TOUCH_STATE_DOWN) // 按下了可以移动动
            {
                g_touch.touch_state |= SC_TOUCH_STATE_MOVE;
                g_touch.obj_focus->handle_event(g_touch.obj_focus, e);
            }
            g_touch.last_x = e->dat.pos[0];
            g_touch.last_y = e->dat.pos[1];
            break;
        case SC_EVENT_TOUCH_UP:
            if (g_touch.touch_state & SC_TOUCH_STATE_DOWN) // 释放事件
            {
                g_touch.obj_focus->handle_event(g_touch.obj_focus, e);
                g_touch.touch_state &= ~(SC_TOUCH_STATE_MOVE | SC_TOUCH_STATE_DOWN);
            }
            break;
        default:
            g_touch.obj_focus->handle_event(g_touch.obj_focus, e);
            break;
        }
        if(g_touch.obj_focus->sc_user_cb)
        {
            g_touch.obj_focus->sc_user_cb(g_touch.obj_focus, e);    // 用户事件回调
        }
    }
}

/// @brief 触摸库主循环
void sc_touch_loop(void)
{
    if (g_touch.srceen == NULL)
        return;
    if (g_touch.anim_cb)
    {
        g_touch.anim_cb(&g_touch); // 触摸动画回调
    }
    uint8_t dirty_cnt = 0;
    sc_area_t *p_dirty = sc_dirty_merge_out(&dirty_cnt);
    if (dirty_cnt == 0)
        return; // 无脏矩形
#if SC_DIRTY_BUCKET_COPY
    /*-------------备份脏矩形,支持绘制使用sc_drity_mark()------*/
    sc_area_t temp_dirty[dirty_cnt];
    for (uint8_t n = 0; n < dirty_cnt; n++)
    {
        temp_dirty[n] = p_dirty[n];
    }
    p_dirty = temp_dirty;
#endif
    // -----------------刷新控件--------------------------------
    sc_obj_t *intersect_obj[20]; // 要绘制的节点
    sc_pfb_t pfb;
    for (uint8_t i = 0; i < dirty_cnt; ++i)
    {
        uint16_t intersect_cnt = 0; // 相交控件
        sc_obj_t *current = g_touch.srceen;
        while (IS_CHILD_OR_SELF(g_touch.srceen, current))
        {
            // 广度搜索不相交+隐藏节点跳过
            sc_area_t cur_area = sc_rect_to_area(&current->rect);
            if (sc_area_nor_intersect(&cur_area, &p_dirty[i]) || (current->attr & SC_OBJ_ATTR_HIDDEN))
            {
                current = sc_obj_next_tree(current);
            }
            else
            {
                if (intersect_cnt < sizeof(intersect_obj) / sizeof(intersect_obj[0]))
                {
                    current->flag &= ~SC_OBJ_FLAG_ACTIVE;     // 清除刷新标记
                    intersect_obj[intersect_cnt++] = current; // 要绘制的节点
                }
                current = current->next;
            }
        }
        // -----------------刷新控件--------------------------------
        sc_event_t event;
        event.type = SC_EVENT_TYPE_DRAW;
        event.dat.arg = &pfb;
        sc_pfb_init_slices(&pfb, &p_dirty[i], gui->bkc);
        do
        {
            for (int j = 0; j < intersect_cnt; j++)
            {
                sc_obj_t *obj = intersect_obj[j];
                obj->handle_event(obj, &event);
            }
        } while (sc_pfb_next_slice(&pfb)); // 分帧刷新
    }
}
/// 遍历树结构
void sc_obj_list_print(sc_obj_t *root)
{
    if (root == NULL)
        return;
    int obj_cnt = 0;
    sc_obj_t *current = root;
    while (IS_CHILD_OR_SELF(root, current))
    {
        // 打印缩进和节点数据
        for (int i = 0; i < current->level; i++)
        {
            printf("  ");
        }
        const char *name = (current->type < sizeof(obj_name) / sizeof(obj_name[0])) ? obj_name[current->type] : "UNKNOWN";
        printf("|__%s (%d,%d,%d,%d)\n", name, current->rect.x, current->rect.y, current->rect.w, current->rect.h); // 打印节点数据
        current = current->next;
        obj_cnt++;
    }
    printf("obj_cnt=%d\n", obj_cnt);
}
