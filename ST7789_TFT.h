#ifndef ST7789_SPI_XC8_H
#define ST7789_SPI_XC8_H

#include <xc.h>
#include <stdint.h>

#ifndef _XTAL_FREQ
#define _XTAL_FREQ 40000000UL
#endif

#define TFT_WIDTH 240
#define TFT_HEIGHT 240

/* PIC18F4620 control pins */
#define TFT_RST LATEbits.LATE0
#define TFT_DC LATEbits.LATE1
#define TFT_CS LATEbits.LATE2
#define TFT_RST_DIR TRISEbits.TRISE0
#define TFT_DC_DIR TRISEbits.TRISE1
#define TFT_CS_DIR TRISEbits.TRISE2

#define RGB(R, G, B) ((uint16_t)((((uint16_t)(R) & 0xF8) << 8) | (((uint16_t)(G) & 0xFCU) << 3) | (((uint16_t)(B)) >> 3)))

typedef enum
{
	COLOR_BLACK	 = RGB(0, 0, 0),
	COLOR_WHITE	 = RGB(255, 255, 255),
	COLOR_RED	 = RGB(255, 0, 0),
	COLOR_GREEN	 = RGB(0, 255, 0),
	COLOR_BLUE	 = RGB(0, 0, 255),
	COLOR_YELLOW = RGB(255, 255, 0),
	COLOR_CYAN	 = RGB(0, 255, 255),
	COLOR_PINK	 = RGB(255, 192, 203)
} Color;

void ST7789_Init(void);
void ST7789_FillScreen(uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_DrawChar(unsigned char c, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor);
void ST7789_String(const char *string, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor);
void ST7789_DrawCharSize(unsigned char c, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor, uint8_t size);
void ST7789_StringSize(const char *string, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor, uint8_t size);
void ST7789_StringSize_centing(const char *string, uint16_t y, uint16_t color, uint16_t backColor, uint8_t size);

void ST7789_Draw_Opening_Img(void);


#endif
