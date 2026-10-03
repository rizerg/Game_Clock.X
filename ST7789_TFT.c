#include <string.h>
#include "ST7789_TFT.h"
#include "font.h"

#define ST7789_SWRESET 0x01
#define ST7789_SLPOUT 0x11
#define ST7789_NORON 0x13
#define ST7789_INVON 0x21
#define ST7789_DISPON 0x29
#define ST7789_CASET 0x2AU
#define ST7789_RASET 0x2BU
#define ST7789_RAMWR 0x2CU
#define ST7789_MADCTL 0x36
#define ST7789_COLMOD 0x3AU

static void SPI_Write(uint8_t data);
static void ST7789_Command(uint8_t cmd);
static void ST7789_Data(uint8_t data);
static void ST7789_SetWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);

static void SPI_Write(uint8_t data)
{
	uint8_t dummy;

	while (SSPCON1bits.WCOL) SSPCON1bits.WCOL = 0;

	SSPBUF = data;
	while (!SSPSTATbits.BF);

	dummy = SSPBUF; /* BFをクリア */
}

static void ST7789_Command(uint8_t cmd)
{
	TFT_DC = 0;
	TFT_CS = 0;
	SPI_Write(cmd);
	TFT_CS = 1;
}

static void ST7789_Data(uint8_t data)
{
	TFT_DC = 1;
	TFT_CS = 0;
	SPI_Write(data);
	TFT_CS = 1;
}

static void ST7789_SetWindow(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
	// ys += 80;
	// ye += 80;

	ST7789_Command(ST7789_CASET);
	ST7789_Data((uint8_t)(xs >> 8));
	ST7789_Data((uint8_t)xs);
	ST7789_Data((uint8_t)(xe >> 8));
	ST7789_Data((uint8_t)xe);

	ST7789_Command(ST7789_RASET);
	ST7789_Data((uint8_t)(ys >> 8));
	ST7789_Data((uint8_t)ys);
	ST7789_Data((uint8_t)(ye >> 8));
	ST7789_Data((uint8_t)ye);

	ST7789_Command(ST7789_RAMWR);
}

void ST7789_Init(void)
{
	TFT_CS_DIR	= 0;
	TFT_DC_DIR	= 0;
	TFT_RST_DIR = 0;

	TFT_CS	= 1;
	TFT_DC	= 1;
	TFT_RST = 1;

	__delay_ms(20);
	TFT_RST = 0;
	__delay_ms(20);
	TFT_RST = 1;
	__delay_ms(150);

	ST7789_Command(ST7789_SWRESET);
	__delay_ms(200);

	ST7789_Command(ST7789_SLPOUT);
	__delay_ms(150);

	/* RGB565: MCU interfaceもRGB interfaceも16 bit/pixel */
	ST7789_Command(ST7789_COLMOD);
	ST7789_Data(0x55);
	__delay_ms(10);

	/* 秋月モジュールのコネクタが上になる向き。色順はBGR。 */
	ST7789_Command(ST7789_MADCTL);
	ST7789_Data(0x60); /* MY=1, MX=1, BGR=1 */

	/* 240x240 IPSパネルでは反転ONが一般的 */
	ST7789_Command(ST7789_INVON);
	__delay_ms(10);

	ST7789_Command(ST7789_NORON);
	__delay_ms(10);

	ST7789_Command(ST7789_DISPON);
	__delay_ms(100);

	ST7789_FillScreen(COLOR_BLACK);
}

void ST7789_FillScreen(uint16_t color)
{
	uint32_t i;
	uint8_t	 hi = (uint8_t)(color >> 8);
	uint8_t	 lo = (uint8_t)color;

	ST7789_SetWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

	TFT_DC = 1;
	TFT_CS = 0;
	for (i = 0; i < ((uint32_t)TFT_WIDTH * TFT_HEIGHT); i++)
	{
		SPI_Write(hi);
		SPI_Write(lo);
	}
	TFT_CS = 1;
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
	if ((x >= TFT_WIDTH) || (y >= TFT_HEIGHT))
	{
		return;
	}

	ST7789_SetWindow(x, y, x, y);
	TFT_DC = 1;
	TFT_CS = 0;
	SPI_Write((uint8_t)(color >> 8));
	SPI_Write((uint8_t)color);
	TFT_CS = 1;
}

void ST7789_DrawChar(unsigned char c, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor)
{
	uint8_t				 col, row;
	uint8_t				 hi	 = (uint8_t)(color >> 8);
	uint8_t				 lo	 = (uint8_t)color;
	uint8_t				 bhi = (uint8_t)(backColor >> 8);
	uint8_t				 blo = (uint8_t)backColor;
	const unsigned char *glyph;

	if ((c < 0x20) || (c > 0xDFU))
	{
		c = '?';
	}
	if (((x + 5) >= TFT_WIDTH) || ((y + 7) >= TFT_HEIGHT))
	{
		return;
	}

	/* font192[文字コード-0x20][0..4] の5列x8bit形式 */
	glyph = (const unsigned char *)font[c - 0x20];
	ST7789_SetWindow(x, y, x + 5, y + 7);

	TFT_DC = 1;
	TFT_CS = 0;
	for (row = 0; row < 8; row++)
	{
		for (col = 0; col < 6; col++)
		{
			uint8_t on = 0;
			if (col < 5)
			{
				on = (uint8_t)((glyph[col] >> row) & 0x01);
			}
			if (on)
			{
				SPI_Write(hi);
				SPI_Write(lo);
			}
			else
			{
				SPI_Write(bhi);
				SPI_Write(blo);
			}
		}
	}
	TFT_CS = 1;
}

void ST7789_String(const char *string, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor)
{
	if (string == 0)
	{
		return;
	}

	while (*string != '\0')
	{
		if (*string == '\n')
		{
			x = 0;
			y += 8;
		}
		else
		{
			if (x + 5 >= TFT_WIDTH)
			{
				x = 0;
				y += 8;
			}
			if (y + 7 >= TFT_HEIGHT)
			{
				break;
			}
			ST7789_DrawChar((unsigned char)*string, x, y, color, backColor);
			x += 6;
		}
		string++;
	}
}

void ST7789_DrawCharSize(uint8_t c, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor, uint8_t size)
{
	uint8_t sourceRow;
	uint8_t sourceCol;
	uint8_t repeatX;
	uint8_t repeatY;
	uint8_t pixelOn;

	uint16_t width;
	uint16_t height;

	uint8_t colorHigh;
	uint8_t colorLow;
	uint8_t backHigh;
	uint8_t backLow;

	const unsigned char *glyph;

	if (size == 0)
	{
		size = 1;
	}

	/*
     * 5x8フォントなので、あまり大きくしすぎないように制限。
     * 必要なら値を変更可能。
     */
	if (size > 8)
	{
		size = 8;
	}

	if ((c < 0x20) || (c > 0xDFU))
	{
		c = (uint8_t)'?';
	}

	/* 文字本体5列＋文字間隔1列 */
	width  = (uint16_t)6 * (uint16_t)size;
	height = (uint16_t)8 * (uint16_t)size;

	/* 画面外に出る文字は描画しない */
	if (x >= TFT_WIDTH || y >= TFT_HEIGHT)
	{
		return;
	}

	if (width > (TFT_WIDTH - x))
	{
		return;
	}

	if (height > (TFT_HEIGHT - y))
	{
		return;
	}

	colorHigh = (uint8_t)(color >> 8);
	colorLow  = (uint8_t)(color & 0xFFU);

	backHigh = (uint8_t)(backColor >> 8);
	backLow	 = (uint8_t)(backColor & 0xFFU);

	glyph = (const unsigned char *)&font[c - 0x20][0];

	ST7789_SetWindow(x, y, x + width - 1, y + height - 1);

	TFT_DC = 1;
	TFT_CS = 0;

	/*
     * ST7789は、設定したウィンドウに対して
     * 左上から右方向、次に下方向の順で書き込む。
     */
	for (sourceRow = 0; sourceRow < 8; sourceRow++)
	{
		/* 元フォントの1行を縦方向へ拡大 */
		for (repeatY = 0; repeatY < size; repeatY++)
		{
			/* フォント5列＋右側の空白1列 */
			for (sourceCol = 0; sourceCol < 6; sourceCol++)
			{
				if (sourceCol < 5)
				{
					pixelOn = (uint8_t)((glyph[sourceCol] >> sourceRow) & 0x01);
				}
				else
				{
					/* 6列目は文字間隔 */
					pixelOn = 0;
				}

				/* 元フォントの1ドットを水平方向へ拡大 */
				for (repeatX = 0; repeatX < size; repeatX++)
				{
					if (pixelOn != 0)
					{
						SPI_Write(colorHigh);
						SPI_Write(colorLow);
					}
					else
					{
						SPI_Write(backHigh);
						SPI_Write(backLow);
					}
				}
			}
		}
	}

	TFT_CS = 1;
}


void ST7789_StringSize(const char *string, uint16_t x, uint16_t y, uint16_t color, uint16_t backColor, uint8_t size)
{
	uint16_t startX;
	uint16_t charWidth;
	uint16_t charHeight;
	uint8_t	 code;

	if (string == 0)
	{
		return;
	}

	if (size == 0)
	{
		size = 1;
	}

	if (size > 8)
	{
		size = 8;
	}

	startX = x;

	charWidth = (uint16_t)6 * (uint16_t)size;

	charHeight = (uint16_t)8 * (uint16_t)size;

	while (*string != '\0')
	{
		/*
         * 改行処理
         */
		if (*string == '\n')
		{
			x = startX;

			if (charHeight > (TFT_HEIGHT - y))
			{
				break;
			}

			y += charHeight;
			string++;
			continue;
		}

		/*
         * 右端を超える場合は自動改行
         */
		if (x >= TFT_WIDTH || charWidth > (TFT_WIDTH - x))
		{
			x = startX;

			if (charHeight > (TFT_HEIGHT - y))
			{
				break;
			}

			y += charHeight;
		}

		/*
         * 下端を超える場合は終了
         */
		if (y >= TFT_HEIGHT || charHeight > (TFT_HEIGHT - y))
		{
			break;
		}

		/*
         * XC8でcharが符号付きの場合に備えて、
         * 必ずuint8_tへ変換する。
         */
		code = (uint8_t)(unsigned char)*string;

		ST7789_DrawCharSize(code, x, y, color, backColor, size);

		x += charWidth;
		string++;
	}
}


void ST7789_StringSize_centing(const char *string, uint16_t y, uint16_t color, uint16_t backColor, uint8_t size)
{
	ST7789_StringSize(string, (240 - (6 * size * strlen(string))) / 2, y, color, backColor, size);
}


void ST7789_Draw_Opening_Img(void)
{
	ST7789_SetWindow(74, 35, 74 + 92, 35 + 64);

	TFT_DC = 1;
	TFT_CS = 0;
	for (uint32_t i = 0; i < (93 * 65); i++)
	{
		SPI_Write((uint8_t)(Opening_img[i] >> 8));
		SPI_Write((uint8_t)(Opening_img[i]));
	}
	TFT_CS = 1;
}