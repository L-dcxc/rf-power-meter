//增加index——QOI格式
//兼容 WeGui RGB 压缩图片格式
//工具下载：https://github.com/KOUFU-DIY/LCD_MCU_TOOL
//开源地址https://github.com/KOUFU-DIY/WeGui_RGB
// 2026-03-17
#include "sc_gui.h"
// 辅助函数：根据空降序号读取字节偏移量
static uint32_t get_qoi_offset(const uint8_t *index_base, uint16_t u16_size, uint16_t u24_size, uint16_t u32_size, uint32_t interval_idx)
{
    uint32_t u16_cnt = u16_size / 2;
    uint32_t u24_cnt = u24_size / 3;
    if (interval_idx < u16_cnt)
    {
        const uint8_t *p = index_base + interval_idx * 2;
        return (p[0] << 8) | p[1];
    }
    else if (interval_idx < u16_cnt + u24_cnt)
    {
        uint32_t idx = interval_idx - u16_cnt;
        const uint8_t *p = index_base + u16_size + idx * 3;
        return (p[0] << 16) | (p[1] << 8) | p[2];
    }
    else
    {
        uint32_t idx = interval_idx - u16_cnt - u24_cnt;
        const uint8_t *p = index_base + u16_size + u24_size + idx * 4;
        return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    }
}



// 绘制 indexQOI 到 PFB
void sc_draw_Image_indexQOI(sc_pfb_t *dest, int xs, int ys, const uint8_t *src, uint8_t alpha)
{
    sc_pfb_t tpfb; // 临时pfb
    sc_area_t intersection;
    uint16_t w = (src[1] << 8) | src[2];
    uint16_t h = (src[3] << 8) | src[4];
    sc_area_t img_area = {xs, ys, xs + w, ys + h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &img_area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &img_area, &intersection))    // 1. 求解交集，无交集直接返回
        return;
    // 2. 解析文件头部
    uint8_t head_size = src[0];
    uint16_t interval = (src[5] << 8) | src[6]; // 索引间隔
    uint16_t u16_size = (src[7] << 8) | src[8];
    uint16_t u24_size = (src[9] << 8) | src[10];
    uint16_t u32_size = (src[11] << 8) | src[12];
    const uint8_t *index_base = src + head_size;
    const uint8_t *qoi_data = index_base + u16_size + u24_size + u32_size;
    do
    {
        uint16_t *dest_out = &dest->buf[(intersection.ys - (dest->y)) * dest->w - dest->x];
         // 3. 计算 PFB Y起始/结束行
        for (int y = intersection.ys; y < intersection.ye; y++)
        {
            // 4. 定位空降点
            uint32_t start_interval = (y - ys);
            uint32_t offset = get_qoi_offset(index_base, u16_size, u24_size, u32_size, start_interval);
            const uint8_t *p = qoi_data + offset;
            // 5. 解码状态初始化
            uint8_t r = 0, g = 0, b = 0;
            uint16_t cur_pixel = 0;
            uint32_t run = 0;
            for (int x = img_area.xs; x < img_area.xe; x++) // 遍历整行像素，直到行尾
            {
                if (run > 0)
                {
                    run--;
                }
                else
                {
                    uint8_t flag = *p++;
                    if (flag == 0xFF || flag == 0xFE)
                    {
                        // RGB565 绝对值
                        uint8_t h_val = *p++;
                        uint8_t l_val = *p++;
                        cur_pixel = (h_val << 8) | l_val;
                        r = h_val >> 3;
                        g = ((h_val & 0x07) << 3) | (l_val >> 5);
                        b = l_val & 0x1F;
                    }
                    else if ((flag & 0xC0) == 0x40)
                    {
                        // 小差值 QOI_OP_DIFF
                        r = (r + ((flag >> 4) & 0x03) - 2) & 0x1F;
                        g = (g + ((flag >> 2) & 0x03) - 2) & 0x3F;
                        b = (b + (flag & 0x03) - 2) & 0x1F;
                        cur_pixel = (r << 11) | (g << 5) | b;
                    }
                    else if ((flag & 0xC0) == 0x80)
                    {
                        // 大差值 QOI_OP_LUMA
                        int8_t vg = (flag & 0x3F) - 32;
                        uint8_t next_byte = *p++;
                        r = (r + vg - 8 + ((next_byte >> 4) & 0x0F)) & 0x1F;
                        g = (g + vg) & 0x3F;
                        b = (b + vg - 8 + (next_byte & 0x0F)) & 0x1F;
                        cur_pixel = (r << 11) | (g << 5) | b;
                    }
                    else if ((flag & 0xC0) == 0xC0)
                    {
                        // 重复码 QOI_OP_RUN
                        run = flag & 0x3F; // QOI存储的是 run - 1，所以这里刚好等于剩余需要填充的次数
                    }
                }
                if (x >= intersection.xs)
                {
                    if (x < intersection.xe)
                    {
                        dest_out[x] = cur_pixel; // 写入像素
                    }
                    else
                    {
                        break;
                    }
                }
            }
            dest_out += dest->w; // 移动到下一行
        }
    } while((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &img_area, &intersection));
}



