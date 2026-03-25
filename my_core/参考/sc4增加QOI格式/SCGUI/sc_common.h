#ifndef SC_COMMON_H
#define SC_COMMON_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "lvgl.h"

/* 全局屏幕配置（统一修改）*/
#define PY32_MCU 0

#if PY32_MCU
#define SC_SCREEN_WIDTH (160)
#define SC_SCREEN_HEIGHT (80)
#define SC_PFB_BUF_SIZE (SC_SCREEN_WIDTH * 15) // 示例：10行高度
#define SC_LCD_DMA_2BUF (1)                    // 是否启用DMA双buf传输
#define SC_LCD_DMA_WAP (1)                     // 是否DMA传输时高低位WAP
#else
#define SC_SCREEN_WIDTH (280)
#define SC_SCREEN_HEIGHT (240)
#define SC_PFB_BUF_SIZE (SC_SCREEN_WIDTH * 2) // 示例：10行高度
#define SC_LCD_DMA_2BUF (1)                    // 是否启用DMA双buf传输
#define SC_LCD_DMA_WAP (0)                     // 是否DMA传输时高低位WAP
#endif

/*lvgl字体配置*/
#define SC_UINCODE_U16 (1)   // 字库总字数:<65535
#define SC_UINCODE_SIZE (64) // 单行文体缓存局部数组,默认:[64]超出自动换行

/* 脏区管理配置，单桶8字节*/
#define SC_DIRTY_BUCKET_N (8)             // 脏桶数量，用满强制合并到第一个，多个控件同时更新时适当改大
#define SC_DIRTY_AREA_THRESHOLD (15 * 15) // 脏桶相交大于阀值面积合并，脏桶数量剩余一半时为0
#define SC_DIRTY_BUCKET_COPY (0)          // 脏桶临时拷贝，默认:0不拷贝

/* 内存管理 */
#define sc_memset memset
#define sc_malloc(size) malloc(size)
#define sc_realloc(p, size) realloc(p, size)
#define sc_free(p) free(p)

/* 工具宏 */
#define SC_POW2(x) ((x) * (x))
#define SC_Q15 (1 << 15)
#define SC_INT16_MAX (32767)
#define SC_INT16_MIN (-32767)
#define SC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SC_MAX(a, b) ((a) > (b) ? (a) : (b))
#define SC_ABS(value) ((value) > 0 ? (value) : -(value))
#define SC_CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

/* 跨编译器 __weak*/
#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4))
#define SC_WEAK __attribute__((weak))
#else
#define SC_WEAK __weak
#endif

/*跨编译器内存对齐宏 */
#if defined(__ICCARM__)
#define SC_ALIGN_ARRAY(type, name, size, boundary) _Pragma("data_alignment=" #boundary) type name[size]
#elif defined(__CC_ARM)
#define SC_ALIGN_ARRAY(type, name, size, boundary) __align(boundary) type name[size]
#elif defined(__GNUC__) || defined(__clang__)
#define SC_ALIGN_ARRAY(type, name, size, boundary) type name[size] __attribute__((aligned(boundary)))
#else
#define ALIGN_ARRAY(type, name, size, boundary) type name[size]
#endif

/* =============类型定义================== */
/*字符编码**/
#if SC_UINCODE_U16
typedef uint16_t unicode_t;
#define UINCODE_MAX (0xFFFF)
#else
typedef uint32_t unicode_t;
#define UINCODE_MAX (0xFFFFFFFF)
#endif

/*颜色类型原生 */
typedef uint16_t color_t;

/* 区域结构体 */
typedef struct
{
    int16_t xs;
    int16_t ys;
    int16_t xe;
    int16_t ye;
} sc_area_t;

/* 矩形结构体 */
typedef struct
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
} sc_rect_t;

/* PFB结构体 */
typedef struct sc_pfb_t
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    int16_t y_st;
    int16_t y_end;
    color_t color;     // 缓冲区颜色
    color_t *buf;      // 缓冲区
    sc_rect_t *parent; // 裁剪区域
} sc_pfb_t;

/* =============外部内联工具 ================== */
/*合并两个rect区域*/
static inline sc_rect_t sc_rect_merge(const sc_rect_t *a, const sc_rect_t *b)
{
    sc_rect_t m = {0};
    m.x = SC_MIN(a->x, b->x);
    m.y = SC_MIN(a->y, b->y);
    m.w = SC_MAX(a->x + a->w, b->x + b->w) - m.x;
    m.h = SC_MAX(a->y + a->h, b->y + b->h) - m.y;
    return m;
}


// 矩形->区域
static inline sc_area_t sc_rect_to_area(sc_rect_t *rect)
{
    return (sc_area_t){
        .xs = rect->x,
        .ys = rect->y,
        .xe = rect->x + rect->w,
        .ye = rect->y + rect->h,
    };
}
// 区域->矩形
static inline sc_rect_t sc_area_to_rect(sc_area_t *area)
{
    return (sc_rect_t){
        .x = area->xs,
        .y = area->ys,
        .w = area->xe - area->xs,
        .h = area->ye - area->ys,
    };
}

/*点是否在区域内*/
static inline bool sc_rect_touch_ctx(sc_rect_t *a, int x, int y)
{
    return (x >= a->x && x < a->x + a->w && y >= a->y && y < a->y + a->h);
}

//*区域无相交*/
static inline bool sc_area_nor_intersect(const sc_area_t *a, const sc_area_t *b)
{
    return (a->xe <= b->xs || a->xs >= b->xe ||
            a->ye <= b->ys || a->ys >= b->ye);
}

/*矩形无相交*/
static inline bool sc_rect_nor_intersect(const sc_rect_t *a, const sc_rect_t *b)
{
    return (a->x + a->w <= b->x || a->x >= b->x + b->w ||
            a->y + a->h <= b->y || a->y >= b->y + b->h);
}

/*矩形相交A+B=重叠区域*/
static inline bool sc_rect_intersect_to_area(const sc_rect_t *a, const sc_rect_t *b, sc_area_t *out)
{
    if (sc_rect_nor_intersect(a, b))
        return false;
    out->ys = SC_MAX(a->y, b->y);
    out->ye = SC_MIN(a->y + a->h, b->y + b->h);
    out->xs = SC_MAX(a->x, b->x);
    out->xe = SC_MIN(a->x + a->w, b->x + b->w);
    return true;
}
//*重叠区域->parent区域*/
static inline bool sc_area_intersect_to_parent(sc_area_t *out, const sc_rect_t *parent)
{
    if (parent)
    {
        out->xs = SC_MAX(out->xs, parent->x);
        out->ys = SC_MAX(out->ys, parent->y);
        out->xe = SC_MIN(out->xe, parent->x + parent->w);
        out->ye = SC_MIN(out->ye, parent->y + parent->h);
        if (out->xs >= out->xe || out->ys >= out->ye)
        {
            return false;
        }
    }
    return true;
}
/*标记脏矩形*/
void sc_dirty_mark(const sc_rect_t *parent, const sc_rect_t *dirty);

/*合并脏区域输出*/
sc_area_t *sc_dirty_merge_out(uint8_t *valid_cnt);

/*初始化第一个切片*/
bool sc_pfb_init_slices(sc_pfb_t *dest, sc_area_t *dirty, color_t color);

/*pfb相交判断*/
bool sc_pfb_intersection(const sc_pfb_t *dest, const sc_area_t *area, sc_area_t *out);

/*切换下一个切片*/
bool sc_pfb_next_slice(sc_pfb_t *handle);

/*透明度混合*/
uint16_t alphaBlend(uint16_t fc, uint16_t bc, uint16_t alpha);

///**fun:打点画线*/
void _sc_draw_Line(int x1, int y1, int x2, int y2, uint16_t colour);

/*三角函数*/
int32_t sc_cos(int16_t angle);

/*三角函数*/
int32_t sc_sin(int16_t angle);

///---------------------色彩定义--------------------------------
#define C_RGB(r, g, b) ((r >> 3) << 11 | (g >> 2) << 5 | b >> 3)
#define C_MAROON 0x8000
#define C_DARK_RED 0x8800
#define C_BROWN 0xA145
#define C_FIREBRICK 0xB104
#define C_CRIMSON 0xD8A7
#define C_RED 0xF800
#define C_TOMATO 0xFB09
#define C_CORAL 0xFBEA
#define C_INDIAN_RED 0xCAEB
#define C_LIGHT_CORAL 0xEC10
#define C_DARK_SALMON 0xE4AF
#define C_SALMON 0xF40E
#define C_LIGHT_SALMON 0xFD0F
#define C_ORANGE_RED 0xFA20
#define C_DARK_ORANGE 0xFC60
#define C_ORANGE 0xFD20
#define C_GOLD 0xFEA0
#define C_DARK_GOLDEN_ROD 0xB421
#define C_GOLDEN_ROD 0xDD24
#define C_PALE_GOLDEN_ROD 0xEF35
#define C_DARK_KHAKI 0xBDAD
#define C_KHAKI 0xEF31
#define C_OLIVE 0x8400
#define C_YELLOW 0xFFE0
#define C_YELLOW_GREEN 0x9E66
#define C_DARK_OLIVE_GREEN 0x5346
#define C_OLIVE_DRAB 0x6C64
#define C_LAWN_GREEN 0x7FC0
#define C_CHART_REUSE 0x7FE0
#define C_GREEN_YELLOW 0xAFE6
#define C_DARK_GREEN 0x0320
#define C_GREEN 0x07E0
#define C_FOREST_GREEN 0x2444

#define C_LIME 0x07E0
#define C_LIME_GREEN 0x3666
#define C_LIGHT_GREEN 0x9772
#define C_PALE_GREEN 0x97D2
#define C_DARK_SEA_GREEN 0x8DD1
#define C_MEDIUM_SPRING_GREEN 0x07D3
#define C_SPRING_GREEN 0x07EF
#define C_SEA_GREEN 0x344B
#define C_MEDIUM_AQUA_MARINE 0x6675
#define C_MEDIUM_SEA_GREEN 0x3D8E
#define C_LIGHT_SEA_GREEN 0x2595
#define C_DARK_SLATE_GRAY 0x328A
#define C_TEAL 0x0410
#define C_DARK_CYAN 0x0451
#define C_AQUA 0x07FF
#define C_CYAN 0x07FF
#define C_LIGHT_CYAN 0xDFFF
#define C_DARK_TURQUOISE 0x0679
#define C_TURQUOISE 0x46F9
#define C_MEDIUM_TURQUOISE 0x4E99
#define C_PALE_TURQUOISE 0xAF7D
#define C_AQUA_MARINE 0x7FFA

#define C_POWDER_BLUE 0xAEFC
#define C_CADET_BLUE 0x64F3
#define C_STEEL_BLUE 0x4C16
#define C_CORN_FLOWER_BLUE 0x64BD
#define C_DEEP_SKY_BLUE 0x05FF
#define C_DODGER_BLUE 0x249F
#define C_LIGHT_BLUE 0xAEBC
#define C_SKY_BLUE 0x867D
#define C_LIGHT_SKY_BLUE 0x867E
#define C_MIDNIGHT_BLUE 0x18CE
#define C_NAVY 0x0010
#define C_DARK_BLUE 0x0011
#define C_MEDIUM_BLUE 0x0019
#define C_BLUE 0x001F
#define C_ROYAL_BLUE 0x435B
#define C_BLUE_VIOLET 0x897B
#define C_INDIGO 0x4810
#define C_DARK_SLATE_BLUE 0x49F1
#define C_SLATE_BLUE 0x6AD9
#define C_MEDIUM_SLATE_BLUE 0x7B5D
#define C_MEDIUM_PURPLE 0x939B
#define C_DARK_MAGENTA 0x8811
#define C_DARK_VIOLET 0x901A
#define C_DARK_ORCHID 0x9999
#define C_MEDIUM_ORCHID 0xBABA
#define C_PURPLE 0x8010
#define C_THISTLE 0xD5FA
#define C_PLUM 0xDD1B
#define C_VIOLET 0xEC1D
#define C_MAGENTA 0xF81F
#define C_ORCHID 0xDB9A
#define C_MEDIUM_VIOLET_RED 0xC0B0
#define C_PALE_VIOLET_RED 0xDB92
#define C_DEEP_PINK 0xF8B2
#define C_HOT_PINK 0xFB56
#define C_LIGHT_PINK 0xFDB7
#define C_PINK 0xFDF9
#define C_ANTIQUE_WHITE 0xF75A
#define C_BEIGE 0xF7BB
#define C_BISQUE 0xFF18
#define C_BLANCHED_ALMOND 0xFF59
#define C_WHEAT 0xF6F6
#define C_CORN_SILK 0xFFBB
#define C_LEMON_CHIFFON 0xFFD9
#define C_LIGHT_GOLDEN_ROD_YELLOW 0xF7DA
#define C_LIGHT_YELLOW 0xFFFB
#define C_SADDLE_BROWN 0x8A22
#define C_SIENNA 0x9A85
#define C_CHOCOLATE 0xD344
#define C_PERU 0xCC28
#define C_SANDY_BROWN 0xF52C
#define C_BURLY_WOOD 0xDDB0
#define C_TAN 0xD591
#define C_ROSY_BROWN 0xBC71
#define C_MOCCASIN 0xFF16
#define C_NAVAJO_WHITE 0xFEF5
#define C_PEACH_PUFF 0xFED6
#define C_MISTY_ROSE 0xFF1B
#define C_LAVENDER_BLUSH 0xFF7E
#define C_LINEN 0xF77C
#define C_OLD_LACE 0xFFBC
#define C_PAPAYA_WHIP 0xFF7A
#define C_SEA_SHELL 0xFFBD
#define C_MINT_CREAM 0xF7FE
#define C_SLATE_GRAY 0x7412
#define C_LIGHT_SLATE_GRAY 0x7453
#define C_LIGHT_STEEL_BLUE 0xAE1B
#define C_LAVENDER 0xE73E
#define C_FLORAL_WHITE 0xFFDD
#define C_ALICE_BLUE 0xEFBF
#define C_GHOST_WHITE 0xF7BF
#define C_HONEYDEW 0xEFFD
#define C_IVORY 0xFFFD
#define C_AZURE 0xEFFF
#define C_SNOW 0xFFDE
#define C_DIM_GRAY 0x6B4D
#define C_GRAY 0x8410
#define C_DARK_GRAY 0xAD55
#define C_SILVER 0xBDF7
#define C_LIGHT_GRAY 0xD69A
#define C_GAINSBORO 0xDEDB
#define C_WHITE_SMOKE 0xF7BE
#define C_WHITE 0xFFFF
#define C_BLACK 0x0000

#endif // SC_COMMON_H
