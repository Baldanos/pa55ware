/***************************************************
  This is a library for the Adafruit 1.8" SPI display.
  This library works with the Adafruit 1.8" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/358
  as well as Adafruit raw 1.8" TFT display
  ----> http://www.adafruit.com/products/618
 
  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/

/***************************************************
  Teensy 3.0 native version with optimizations
  Modified Jan/2013 by Peter Loveday
****************************************************/

#ifndef _Teensy3_ST7735H_
#define _Teensy3_ST7735H_

#include "Arduino.h"
#include "Print.h"

// some flags for initR() :(
#define INITR_GREENTAB 0x0
#define INITR_REDTAB   0x1
#define INITR_BLACKTAB 0x2

#define ST7735_TFTWIDTH  128
#define ST7735_TFTHEIGHT 160

#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09

#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13

#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E

#define ST7735_PTLAR   0x30
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD

#define ST7735_PWCTR6  0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// Color definitions
#define	ST7735_BLACK   0x0000
#define	ST7735_BLUE    0x001F
#define	ST7735_RED     0xF800
#define	ST7735_GREEN   0x07E0
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_YELLOW  0xFFE0  
#define ST7735_WHITE   0xFFFF

#define RGB(r,g,b) ((b&0xf8)<<8|(g&0xfc)<<4|(r&0xf8)>>3)

class Teensy3_ST7735 : public Print
{
 public:
	 
	// Unlike the adafruit library, these are both hardware SPI,
	// and you must pass valid SPI CS pin for RS, and optionally
	// valid DOUT and SCLK pins. CS and RST can be any digital out
	// pins, (better yet wire RST to the T3's reset).
	// It is possible to create multiple objects if you want more
	// than one display - in this case you can share all pins except
	// CS.

	Teensy3_ST7735(uint8_t CS, uint8_t RS, uint8_t RST = 0);
	Teensy3_ST7735(uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST = 0);

	bool initB(); // for ST7735B displays
	bool initR(uint8_t options = INITR_GREENTAB); // for ST7735R
	void setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);
	void pushColor(uint16_t color);
	void fillScreen(uint16_t color);
	void drawPixel(int16_t x, int16_t y, uint16_t color);
	void _drawPixel(int16_t x, int16_t y, uint16_t color);
	void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
	void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
	void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
	void invertDisplay(boolean i);
	uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);

	void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,  uint16_t color);
	void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,  uint16_t color);

	void drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
	void drawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color);
	void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
	void fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color);

	void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
	void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
	void drawRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);
	void fillRoundRect(int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color);

	void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
	void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);
	void _drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size);
	size_t write(uint8_t);
	void setCursor(int16_t x, int16_t y);
	void setTextColor(uint16_t c);
	void setTextColor(uint16_t c, uint16_t bg);
	void setTextSize(uint8_t s);
	void setTextWrap(boolean w);

	int16_t height();
	int16_t width();

	void setRotation(uint8_t r);
	uint8_t getRotation();

	void spiwait();

protected:
	int16_t  WIDTH, HEIGHT;   // this is the 'raw' display w/h - never changes
	int16_t  _width, _height; // dependent on rotation
	int16_t  cursor_x, cursor_y;
	uint16_t textcolor, textbgcolor;
	uint8_t  textsize;
	uint8_t  rotation;
	boolean  wrap; // If set, 'wrap' text at right edge of display

	uint8_t madctl;
	uint8_t datacs_mask;
	uint8_t cmdcs_mask;

 private:
    uint8_t  tabcolor;
	void commandList(uint8_t *addr);
	bool commonInit(uint8_t *cmdList);
	void init(uint8_t CS, uint8_t RS, uint8_t SID, uint8_t SCLK, uint8_t RST);

	uint8_t _cs, _rs, _rst, _sid, _sclk;
    uint8_t colstart, rowstart;

	uint32_t ctar0, ctar1, mcr;
};

#endif
