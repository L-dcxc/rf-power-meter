/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ST7567A.c
  * @brief   ST7735S TFT LCD Driver for STM32
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ST7567A.h"
#include "main.h"
#include "spi.h"
#include "tim.h"
#include "oledfont.h"
#include "string.h"

/* USER CODE BEGIN 0 */
// LCD??????????????
uint16_t POINT_COLOR = 0xFFFF; // ???????
uint16_t BACK_COLOR = 0x0000;  // ?????
// ????LCD???????
// ????????
_lcd_dev lcddev;
/* USER CODE END 0 */

/* USER CODE BEGIN 1 */
/*****************************************************************************
 * @name       :void LCD_SPI_WriteByte(uint8_t data)
 * @date       :2025-01-09
 * @function   :Write a byte of data using STM32 Hardware SPI
 * @parameters :data:Data to be written
 * @retvalue   :None
******************************************************************************/
void LCD_SPI_WriteByte(uint8_t data) {
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
}
/* USER CODE END 1 */

/* USER CODE BEGIN 2 */
/*****************************************************************************
 * @name       :void LCD_WR_REG(uint8_t Reg)
 * @date       :2025-01-09
 * @function   :Write an 8-bit command to the LCD screen
 * @parameters :Reg:Command value to be written
 * @retvalue   :None
 ******************************************************************************/
void LCD_WR_REG(uint8_t Reg) {
    LCD_A0(0);  // ??????
    LCD_SPI_WriteByte(Reg);
}

/*****************************************************************************
 * @name       :void LCD_WR_DATA(uint8_t Data)
 * @date       :2025-01-09
 * @function   :Write an 8-bit data to the LCD screen
 * @parameters :Data:data value to be written
 * @retvalue   :None
 ******************************************************************************/
void LCD_WR_DATA(uint8_t Data) {
    LCD_A0(1);  // ??????
    LCD_SPI_WriteByte(Data);
}

/*****************************************************************************
 * @name       :void LCD_WR_DATA_16Bit(uint16_t Data)
 * @date       :2025-01-09
 * @function   :Write an 16-bit data to the LCD screen
 * @parameters :Data:Data to be written
 * @retvalue   :None
 ******************************************************************************/
void LCD_WR_DATA_16Bit(uint16_t Data) {
    LCD_A0(1);  // ??????
    LCD_SPI_WriteByte(Data >> 8);
    LCD_SPI_WriteByte(Data & 0xFF);
}

/*****************************************************************************
 * @name       :void LCD_WriteReg(uint8_t LCD_Reg, uint8_t LCD_RegValue)
 * @date       :2025-01-09
 * @function   :Write data into registers
 * @parameters :LCD_Reg:Register address
                LCD_RegValue:Data to be written
 * @retvalue   :None
******************************************************************************/
void LCD_WriteReg(uint8_t LCD_Reg, uint8_t LCD_RegValue) {
    LCD_WR_REG(LCD_Reg);
    LCD_WR_DATA(LCD_RegValue);
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
/*****************************************************************************
 * @name       :void LCD_WriteRAM_Prepare(void)
 * @date       :2025-01-09
 * @function   :Write GRAM
 * @parameters :None
 * @retvalue   :None
 ******************************************************************************/
void LCD_WriteRAM_Prepare(void) {
    LCD_WR_REG(lcddev.wramcmd);
}

/*****************************************************************************
 * @name       :void LCD_Clear(uint16_t Color)
 * @date       :2025-01-09
 * @function   :Full screen filled LCD screen
 * @parameters :Color:Filled color
 * @retvalue   :None
 ******************************************************************************/
void LCD_Clear(uint16_t Color) {
    uint16_t i, j;
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
    for (i = 0; i < lcddev.width; i++) {
        for (j = 0; j < lcddev.height; j++) {
            LCD_WR_DATA_16Bit(Color);
        }
    }
}

/*****************************************************************************
 * @name       :void LCD_DrawPoint(uint16_t x, uint16_t y)
 * @date       :2025-01-09
 * @function   :Write a pixel data at a specified location
 * @parameters :x:the x coordinate of the pixel
                y:the y coordinate of the pixel
 * @retvalue   :None
******************************************************************************/
void LCD_DrawPoint(uint16_t x, uint16_t y) {
    LCD_SetCursor(x, y);
    LCD_WR_DATA_16Bit(POINT_COLOR);
}

/*****************************************************************************
 * @name       :void LCD_Reset(void)
 * @date       :2025-01-09
 * @function   :Reset LCD screen
 * @parameters :None
 * @retvalue   :None
 ******************************************************************************/
void LCD_Reset(void) {
    LCD_RESET(1);
    HAL_Delay(50);
    LCD_RESET(0);
    HAL_Delay(50);
    LCD_RESET(1);
    HAL_Delay(50);
}
/* USER CODE END 3 */

/* USER CODE BEGIN 4 */
/*****************************************************************************
 * @name       :void LCD_Init(void)
 * @date       :2025-01-09
 * @function   :Initialize ST7735S LCD
 * @parameters :None
 * @retvalue   :None
 ******************************************************************************/
void LCD_Init(void) {
    uint8_t senddata1[] = {
        0x04, 0x22, 0x07, 0x0A, 0x2E, 0x30, 0x25, 0x2A,
        0x28, 0x26, 0x2E, 0x3A, 0x00, 0x01, 0x03, 0x13
    };
    uint8_t senddata2[] = {
        0x04, 0x16, 0x06, 0x06, 0x0D, 0x2D, 0x26, 0x23,
        0x27, 0x27, 0x25, 0x2D, 0x3B, 0x00, 0x01, 0x04,
        0x13
    };
    uint8_t i = 0;

    // ?????? (PWM?????????????????????)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, 5000); // 50%????

    LCD_Reset(); // ?????????¦Ë

    //************* ST7735S?????**********//
    LCD_WR_REG(0x11); // Sleep exit
    HAL_Delay(120);

    LCD_WR_REG(0xB1);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x3c);
    LCD_WR_DATA(0x3c);

    LCD_WR_REG(0xB2);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x3c);
    LCD_WR_DATA(0x3c);

    LCD_WR_REG(0xB3);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x3c);
    LCD_WR_DATA(0x3c);
    LCD_WR_DATA(0x05);
    LCD_WR_DATA(0x3c);
    LCD_WR_DATA(0x3c);

    LCD_WR_REG(0xB4);
    LCD_WR_DATA(0x03);

    LCD_WR_REG(0xC0);  // Set VRH1[4:0] & VC[2:0] for VCI1 & GVDD
    LCD_WR_DATA(0x28);
    LCD_WR_DATA(0x08);
    LCD_WR_DATA(0x04);

    LCD_WR_REG(0xC1); // Set BT[2:0] for AVDD & VCL & VGH & VGL
    LCD_WR_DATA(0xC0);

    LCD_WR_REG(0xC2);  // Set VMH[6:0] & VML[6:0] for VOMH & VCOML
    LCD_WR_DATA(0x0D);
    LCD_WR_DATA(0x00);

    LCD_WR_REG(0xC3);
    LCD_WR_DATA(0x8D);
    LCD_WR_DATA(0x2A);

    LCD_WR_REG(0xC4);
    LCD_WR_DATA(0x8D);
    LCD_WR_DATA(0xEE);

    LCD_WR_REG(0xC5);
    LCD_WR_DATA(0x1A);

    LCD_WR_REG(0x36); // MX,MY,RGB MODE
    LCD_WR_DATA(0xc8);

    LCD_WR_REG(0xe0);
    for (i = 0; i < sizeof(senddata1) / sizeof(senddata1[0]); i++) {
        LCD_WR_DATA(senddata1[i]);
    }

    LCD_WR_REG(0xe1);
    for (i = 0; i < sizeof(senddata2) / sizeof(senddata2[0]); i++) {
        LCD_WR_DATA(senddata2[i]);
    }

    LCD_WR_REG(0x3A);
    LCD_WR_DATA(0x05);

    LCD_WR_REG(0x29); // Display On

    // ????LCD???????
    LCD_Direction(); // ????LCD???????
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
/*****************************************************************************
 * @name       :void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd)
 * @date       :2025-01-09
 * @function   :Setting LCD display window
 * @parameters :xStar:the beginning x coordinate of the LCD display window
                yStar:the beginning y coordinate of the LCD display window
                xEnd:the ending x coordinate of the LCD display window
                yEnd:the ending y coordinate of the LCD display window
 * @retvalue   :None
******************************************************************************/
void LCD_SetWindows(uint16_t xStar, uint16_t yStar, uint16_t xEnd, uint16_t yEnd) {
    // ???????????
    xStar += LCD_X_OFFSET;
    yStar += LCD_Y_OFFSET;
    xEnd += LCD_X_OFFSET;
    yEnd += LCD_Y_OFFSET;

    LCD_WR_REG(lcddev.setxcmd);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(xStar);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(xEnd);

    LCD_WR_REG(lcddev.setycmd);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(yStar);
    LCD_WR_DATA(0x00);
    LCD_WR_DATA(yEnd);

    LCD_WriteRAM_Prepare(); // ???§Õ??GRAM
}

/*****************************************************************************
 * @name       :void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
 * @date       :2025-01-09
 * @function   :Set coordinate value
 * @parameters :Xpos:the x coordinate of the pixel
                Ypos:the y coordinate of the pixel
 * @retvalue   :None
******************************************************************************/
void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos) {
    LCD_SetWindows(Xpos, Ypos, Xpos, Ypos);
}

/*****************************************************************************
 * @name       :void LCD_Direction(void)
 * @date       :2025-01-09
 * @function   :Setting the display direction of LCD screen
 * @parameters :None
 * @retvalue   :None
******************************************************************************/
void LCD_Direction(void) {
    lcddev.setxcmd = 0x2A;
    lcddev.setycmd = 0x2B;
    lcddev.wramcmd = 0x2C;
    lcddev.width = LCD_W;
    lcddev.height = LCD_H;
    LCD_WriteReg(0x36, (0 << 3) | (1 << 7) | (0 << 6) | (1 << 5)); // MY=1,MX=0,MV=1 
}

/*****************************************************************************
 * @name       :void LCD_SetBacklight(uint16_t brightness)
 * @date       :2025-01-09
 * @function   :Set LCD backlight brightness
 * @parameters :brightness: 0-10000 (0=off, 10000=max brightness)
 * @retvalue   :None
******************************************************************************/
void LCD_SetBacklight(uint16_t brightness) {
    if (brightness >= 999) brightness = 999;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, brightness);
}

/*****************************************************************************
 * @name       :void LCD_ShowChar(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t num, uint8_t size, uint8_t mode)
 * @date       :2025-01-09
 * @function   :Display a single English character
 * @parameters :x,y:coordinate
                fc:font color
                bc:background color
                num:character to display
                size:font size 12/16
                mode:0-normal,1-overlay
 * @retvalue   :None
******************************************************************************/
void LCD_ShowChar(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t num, uint8_t size, uint8_t mode) {
    uint8_t temp;
    uint8_t pos, t;
    uint16_t colortemp = POINT_COLOR;

    num = num - ' '; // ?????????
    LCD_SetWindows(x, y, x + size / 2 - 1, y + size - 1); // ??????????????????
    if (!mode) { // ???????
        for (pos = 0; pos < size; pos++) {
            if (size == 12)
                temp = asc2_1206[num][pos]; // ????1206????
            else
                temp = asc2_1608[num][pos]; // ????1608????
            for (t = 0; t < size / 2; t++) {
                if (temp & 0x01)
                    LCD_WR_DATA_16Bit(fc);
                else
                    LCD_WR_DATA_16Bit(bc);
                temp >>= 1;
            }
        }
    } else { // ??????
        for (pos = 0; pos < size; pos++) {
            if (size == 12)
                temp = asc2_1206[num][pos]; // ????1206????
            else
                temp = asc2_1608[num][pos]; // ????1608????
            for (t = 0; t < size / 2; t++) {
                POINT_COLOR = fc;
                if (temp & 0x01)
                    LCD_DrawPoint(x + t, y + pos); // ???????
                temp >>= 1;
            }
        }
    }
    POINT_COLOR = colortemp;
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // ???????????
}

/*****************************************************************************
 * @name       :void Show_Str(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *str, uint8_t size, uint8_t mode)
 * @date       :2025-01-09
 * @function   :Display English string
 * @parameters :x,y:coordinate
                fc:font color
                bc:background color
                str:string to display
                size:font size 12/16
                mode:0-normal,1-overlay
 * @retvalue   :None
******************************************************************************/
void Show_Str(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *str, uint8_t size, uint8_t mode) {
    uint16_t x0 = x;
    uint8_t bHz = 0; // ?????????

    while (*str != 0) { // ????¦Ä????
        if (!bHz) {
            if (x > (lcddev.width - size / 2) || y > (lcddev.height - size))
                return;
            if (*str > 0x80)
                bHz = 1; // ????
            else { // ???
                if (*str == 0x0D) { // ???§Ù???
                    y += size;
                    x = x0;
                    str++;
                } else {
                    if (size > 16) { // ???????§Þ???12X24 16X32????????,??8X16???
                        LCD_ShowChar(x, y, fc, bc, *str, 16, mode);
                        x += 8; // ???,????????
                    } else {
                        LCD_ShowChar(x, y, fc, bc, *str, size, mode);
                        x += size / 2; // ???,????????
                    }
                }
                str++;
            }
        } else { // ????
            if (x > (lcddev.width - size) || y > (lcddev.height - size))
                return;
            bHz = 0; // ?§Ü????
            if (size == 32)
                ; // GUI_DrawFont32(x, y, fc, bc, str, mode); // ??????32x32
            else if (size == 24)
                ; // GUI_DrawFont24(x, y, fc, bc, str, mode); // ??????24x24
            else
                GUI_DrawFont16(x, y, fc, bc, str, mode);

            str += 2;
            x += size; // ????????????
        }
    }
}

/*****************************************************************************
 * @name       :void GUI_DrawFont16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s, uint8_t mode)
 * @date       :2025-01-09
 * @function   :Display a single 16x16 Chinese character
 * @parameters :x,y: coordinate
                fc: font color
                bc: background color
                s: pointer to Chinese character (2 bytes)
                mode: 0-normal, 1-overlay
 * @retvalue   :None
******************************************************************************/
void GUI_DrawFont16(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, uint8_t *s, uint8_t mode) {
    uint8_t i, j;
    uint16_t k;
    uint16_t HZnum;
    uint16_t colortemp = POINT_COLOR;

    HZnum = givesize_GB16(); // ???????????§³

    // ?????????????????????¦Ä???
    if (HZnum == 0) {
        LCD_SetWindows(x, y, x + 15, y + 15);
        for (i = 0; i < 256; i++) {
            LCD_WR_DATA_16Bit(RED); // ???????????????
        }
        LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1);
        return;
    }

    for (k = 0; k < HZnum; k++) {
        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1))) {
            LCD_SetWindows(x, y, x + 15, y + 15);
            if (!mode) { // ???????
                for (i = 0; i < 32; i++) {
                    for (j = 0; j < 8; j++) {
                        if (tfont16[k].Msk[i] & (0x80 >> j))
                            LCD_WR_DATA_16Bit(fc);
                        else
                            LCD_WR_DATA_16Bit(bc);
                    }
                }
            } else { // ??????
                for (i = 0; i < 32; i++) {
                    for (j = 0; j < 8; j++) {
                        POINT_COLOR = fc;
                        if (tfont16[k].Msk[i] & (0x80 >> j)) {
                            LCD_DrawPoint(x + (i % 2) * 8 + j, y + i / 2);
                        }
                    }
                }
            }
            POINT_COLOR = colortemp;
            LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // ???????????
            return; // ??????????????
        }
    }

    // ¦Ä????????????????????
    LCD_SetWindows(x, y, x + 15, y + 15);
    for (i = 0; i < 256; i++) {
        LCD_WR_DATA_16Bit(BLUE); // ?????????¦Ä??????
    }
    POINT_COLOR = colortemp;
    LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // ???????????
}

/*****************************************************************************
 * @name       :void Show_Chinese(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, char* text, uint8_t size, uint8_t mode)
 * @date       :2025-01-09
 * @function   :Display Chinese string with automatic encoding conversion
 * @parameters :x,y: coordinate
                fc: font color
                bc: background color
                text: Chinese text string (UTF-8)
                size: font size
                mode: 0-normal, 1-overlay
 * @retvalue   :None
******************************************************************************/
void Show_Chinese(uint16_t x, uint16_t y, uint16_t fc, uint16_t bc, char* text, uint8_t size, uint8_t mode) {
    // ??????????? - ??????????????????GB2312????
    typedef struct {
        char* utf8_str;
        uint8_t gb2312_codes[20]; // ???10??????
    } ChineseMap;

    const ChineseMap chinese_map[] = {
        {"???", {0xC6,0xB5, 0xC2,0xCA, 0x00}},
        {"???", {0xD7,0xA4, 0xB2,0xA8, 0x00}},
        {"???", {0xB1,0xC8, 0xD6,0xB5, 0x00}},
        {"?????", {0xD7,0xA4, 0xB2,0xA8, 0xB1,0xC8, 0x00}},
        {"????", {0xD5,0xFD, 0xCF,0xF2, 0x00}},
        {"????", {0xB7,0xB4, 0xC9,0xE4, 0x00}},
        {"????", {0xB4,0xAB, 0xCA,0xE4, 0x00}},
        {"§¹??", {0xD0,0xA7, 0xC2,0xCA, 0x00}},
        {"§µ?", {0xD0,0xA3, 0xD7,0xBC, 0x00}},
        {"????", {0xC9,0xE8, 0xD6,0xC3, 0x00}},
        {"???", {0xB2,0xCB, 0xB5,0xA5, 0x00}},
        {"????", {0xB7,0xB5, 0xBB,0xD8, 0x00}},
        {"???", {0xC8,0xB7, 0xC8,0xCF, 0x00}},
        {"????", {0xB9,0xD8, 0xD3,0xDA, 0x00}},
        {"????", {0xB1,0xA8, 0xBE,0xAF, 0x00}},
        {"????", {0xC1,0xC1, 0xB6,0xC8, 0x00}},
        {"??????", {0xD5,0xFD, 0xCF,0xF2, 0xB9,0xA6, 0xC2,0xCA, 0x00}},
        {"???Êé??", {0xB7,0xB4, 0xC9,0xE4, 0xB9,0xA6, 0xC2,0xCA, 0x00}},
        // ???????????????????...
    };

    uint8_t map_count = sizeof(chinese_map) / sizeof(ChineseMap);
    uint8_t i;

    // ?????????????????
    for (i = 0; i < map_count; i++) {
        if (strcmp(text, chinese_map[i].utf8_str) == 0) {
            Show_Str(x, y, fc, bc, (uint8_t*)chinese_map[i].gb2312_codes, size, mode);
            return;
        }
    }

    // ???????????????"???"
    uint8_t unknown[] = {'?', '?', '?', 0x00};
    Show_Str(x, y, RED, BLACK, unknown, size, mode);
}
/* USER CODE END 5 */
