
#include "sc_event_task.h"
#include "sc_gui.h"
//======================新加多类型事件队列==========================
uint32_t system_tick = 0;               // 全局tick计数器，由中断更新
uint32_t sc_tick = 0;                   // 中间tick变量，在主循环中使用
sc_task_t  sc_task[MAX_THREADS] = {0};
sc_delay_t sc_delay_buf[MAX_THREADS] = {0};
EventQueue eventQueue = {0};
/**fun: 出队列
 *输入: 事件句柄,事件
 *返回:
*/
uint8_t EventQueue_Pop( sc_event_t *event) 
{
    EventQueue* q = &eventQueue;
    if (q->size == 0)
    {
        return 0;  // 队列为空
    }
    *event = q->events[q->head];
    q->head = (q->head + 1) % EVENT_QUEUE_SIZE;
    q->size--;
    return 1;
}

/**fun: 入队列
 *输入: 事件句柄,事件,优先级
 *返回:
*/
uint8_t EventQueue_Push(const sc_event_t *event,uint8_t Priority)
{
    EventQueue* q = &eventQueue;
    if (q->size>= EVENT_QUEUE_SIZE )
    {
        return 0;  // 队列已满
    }
    if(Priority)
    {
        q->head = (EVENT_QUEUE_SIZE-1+q->head) % EVENT_QUEUE_SIZE;
        q->events[q->head] = *event;    //入队列头
    }
    else
    {
        q->events[q->tail] = *event;     //入队列尾
        q->tail=(q->tail+1)%EVENT_QUEUE_SIZE;
    }
    q->size++;
    return 1;
}

/**fun: 创建事件线程,独立id可以和延时共用id
 *输入: 线程ID,ms
 *注:
*/
uint8_t sc_create_task(uint8_t id, void (*thread_cb)(sc_event_t*), uint32_t ms)
{
    sc_event_t e;
    sc_task[id].ms = ms;               //线程定时
    sc_task[id].time_out = system_tick + ms;
    if (sc_task[id].thread_cb != thread_cb)
    {
        sc_task[id].thread_cb = thread_cb;
        /**首次执行初始化事件，建议使用*/
        e.type=SC_EVENT_TYPE_INIT;
        sc_task[id].thread_cb(&e);
        return 0;
    }
    return 1;       //已存在
}

/**fun: 删除线程
 *输入: 线程ID
 *返回:
*/
void sc_delete_task(uint8_t id)
{
    sc_task[id].thread_cb = NULL;
    sc_task[id].ms = 0;
}

/**fun: 单次延时,独立id可以和定时共用id,可以递归使用
 *输入: 线程ID,回调函数,MS
 *返回:
*/
void sc_add_delay(uint8_t id, void (*cb)(sc_delay_t*), uint32_t ms, void *arg)
{
    sc_delay_buf[id].cb = cb;
    sc_delay_buf[id].arg = arg;
    sc_delay_buf[id].time_out = system_tick+ms;
    sc_task[id].delay=&sc_delay_buf[id];
}
/**fun: 停止延时
 *输入: 线程ID
 *返回:
*/
void sc_stop_delay(uint8_t id)
{
    sc_task[id].delay = NULL;
}
/**fun: 主循环调用
*/
void sc_task_loop(void (*user_cb)(sc_event_t *arg))
{
    sc_event_t event={0};
    static uint8_t id=0;
    if(sc_tick!=system_tick)
    {
        sc_tick=system_tick;                    //防止中断打断
        for (id = 0; id < MAX_THREADS; id++)
        {
            sc_task_t *task = &sc_task[id];
            //------------延时回调---------
            if (task->delay)
            {
                uint32_t time_out = task->delay->time_out;  //延时时间
                if((int)time_out-(int)sc_tick<=0)
                {
                    task->delay->cb(task->delay);    //回调重置延时
                    if(time_out==task->delay->time_out)  
                    {
                        task->delay=NULL;  //延时时间未改变则停止延时
                    }
                }
            }
           // ------------定时事件--------
            if (task->ms && (int)task->time_out-(int)sc_tick<=0)
            {
                event.type = SC_EVENT_TYPE_TIMER;
                task->thread_cb(&event);
                task->time_out = system_tick + task->ms;
            }
        }
        while (EventQueue_Pop(&event))  // 处理事件队列中的所有事件
        {
            for (id = 0; id < MAX_THREADS; id++)     // 查找匹配索引
            {
                if (sc_task[id].thread_cb && (event.thread_mask & 0x01))
                {
                    sc_task[id].thread_cb(&event);  // 调用对应的线程函数处理事件
                }
                event.thread_mask>>=1;              // 掩码右移，检查下一个位
            }
            if(user_cb)
            {
                user_cb(&event);
            }
        }
    }

}

/**fun: 发送事件指定线程接收
 *输入: 线程mask,优先级Priority,event可以是指针用long类型
 *返回:
*/
void sc_send_event_mask(uint32_t mask,uint16_t type,uintptr_t event,uint8_t Priority)
{
    sc_event_t e;
    e.thread_mask=mask;     //线程mask
    e.type=type;
    uint8_t* str = (uint8_t*)event;
    int16_t *pos = (int16_t*)event;
    int i=0;
    switch (type)
    {
        case SC_EVENT_TOUCH_DOWN:
        case SC_EVENT_TOUCH_UP:
        case SC_EVENT_TOUCH_MOVE:
        e.dat.pos[0]=pos[0];
        e.dat.pos[1]=pos[1];
        break;
    case SC_EVENT_TYPE_KEY:
        e.dat.key=event;
        break;
    case SC_EVENT_TYPE_CMD:
        e.dat.cmd=event;
        break;
    case SC_EVENT_TYPE_CUSTOM:
        e.dat.arg = (void*)str;
        break;
    case SC_EVENT_TYPE_STR:
        while(*str&&i<MAX_EVENT_DATA_SIZE-1)
        {
            e.dat.str[i++]= *str;
            str++;
        }
        e.dat.str[i]='\0';
        break;
    default:
        break;
    }
    EventQueue_Push( &e,Priority);
}

/**fun:发送广播事件cmd
 *输入:键值
 *返回:
*/
void sc_send_cmd_event(int cmd)
{
    sc_send_event_mask(0xffff,SC_EVENT_TYPE_CMD,cmd,0);
}

void sc_send_pos_event(sc_event_type type, int x,int y)
{
    int16_t pos[2]={x+gui->lcd_rect.x,y+gui->lcd_rect.y};
    sc_send_event_mask((0x01<<0),type,(uintptr_t)&pos,0);
}
