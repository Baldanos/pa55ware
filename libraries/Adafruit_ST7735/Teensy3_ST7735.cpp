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

#include "Teensy3_ST7735.h"
#include <limits.h>
#include <SPI.h>

#include "glcdfont.c"

// I'm not sure if all displays can support 24MHz.
// So if you're having issues, you can try dropping the speed.

//#define BAUD_DIV 0 /* 24MHz SPI */
#define BAUD_DIV 1 /* 12MHz SPI */
//#define BAUD_DIV 2 /* 8MHz SPI */
//#define BAUD_DIV 3 /* 6MHz SPI */
//#define BAUD_DIV 4 /* 3MHz SPI */

#define CS_ON() digitalWrite(_cs, 0); \
		SPI0_CTAR0 = ctar0; SPI0_CTAR1 = ctar1; SPI0_MCR = mcr;

#define CS_OFF() digitalWrite(_cs, 1);

inline uint16_t swapcolor(uint16_t x) { 
      return (x << 11) | (x & 0x07E0) | (x >> 11);
}

Teensy3_ST7735::Teensy3_ST7735(uint8_t cs, uint8_t rs, uint8_t sid, uint8_t sclk, uint8_t rst)
{
	init(cs, rs, sid, sclk, rst);
}


Teensy3_ST7735::Teensy3_ST7735(uint8_t cs, uint8_t rs, uint8_t rst)
{
	init(cs, rs, 0, 0, rst);
}

void Teensy3_ST7735::init(uint8_t cs, uint8_t rs, uint8_t sid, uint8_t sclk, uint8_t rst)
{
	_cs	 = cs;
	_rs	 = rs;
	_sid  = sid;
	_sclk = sclk;
	_rst  = rst;

	_width = WIDTH = ST7735_TFTWIDTH;
	_height = HEIGHT = ST7735_TFTHEIGHT;

	rotation = 0;    
	cursor_y = cursor_x = 0;
	textsize = 1;
	textcolor = textbgcolor = 0xFFFF;
	wrap = true;
	colstart = rowstart = 0;
	
	madctl = 0;
	datacs_mask = 0;
	cmdcs_mask = 0;
}



#define SPI_SR_TXCTR 0x0000f000

#define SPIWRITE1(c, cs) \
	do { \
		while ((SPI0_SR & SPI_SR_TXCTR) >= 0x00004000); \
		SPI0_PUSHR = ((c)&0xff) | SPI_PUSHR_PCS((cs)) | SPI_PUSHR_CTAS(0) | SPI_PUSHR_CONT; \
	} while(0)

#define SPIWRITE2(w, cs) \
	do { \
		while ((SPI0_SR & SPI_SR_TXCTR) >= 0x00004000); \
		SPI0_PUSHR = ((w)&0xffff) | SPI_PUSHR_PCS((cs)) | SPI_PUSHR_CTAS(1) | SPI_PUSHR_CONT; \
	} while(0)

#define SPICMD(c) SPIWRITE1((c), cmdcs_mask)
#define SPIDATA1(c) SPIWRITE1((c), datacs_mask)
#define SPIDATA2(w) SPIWRITE2((w), datacs_mask)

#define SETADDRWINDOW(x0, y0, x1, y1) \
	do { \
	SPICMD(ST7735_CASET); \
	SPIDATA2(uint16_t(x0)+colstart); \
	SPIDATA2(uint16_t(x1)+colstart); \
	SPICMD(ST7735_RASET); \
	SPIDATA2(uint16_t(y0)+rowstart); \
	SPIDATA2(uint16_t(y1)+rowstart); \
	SPICMD(ST7735_RAMWR); \
	} while (0)

#define SPIWAIT() spiwait()

void Teensy3_ST7735::spiwait()
{
	while ((SPI0_SR & SPI_SR_TXCTR) != 0);
	while (!(SPI0_SR & SPI_SR_TCF));
	SPI0_SR |= SPI_SR_TCF;
}


// Rather than a bazillion SPICMD() and SPIDATA() calls, screen
// initialization commands and arguments are organized in these tables
// stored in PROGMEM.	The table may look bulky, but that's mostly the
// formatting -- storage-wise this is hundreds of bytes more compact
// than the equivalent code.	Companion function follows.
#define DELAY 0x80
PROGMEM static prog_uchar
	Bcmd[] = {									// Initialization commands for 7735B screens
		18,                         // 18 commands in list:
		ST7735_SWRESET,   DELAY,    //	1: Software reset, no args, w/delay
			50,                     //		 50 ms delay
		ST7735_SLPOUT ,   DELAY,    //	2: Out of sleep mode, no args, w/delay
			255,                    //		 255 = 500 ms delay
		ST7735_COLMOD , 1+DELAY,    //	3: Set color mode, 1 arg + delay:
			0x05,                   //		 16-bit color
			10,                     //		 10 ms delay
		ST7735_FRMCTR1, 3+DELAY,    //	4: Frame rate control, 3 args + delay:
			0x00,                   //		 fastest refresh
			0x06,                   //		 6 lines front porch
			0x03,                   //		 3 lines back porch
			10,                     //		 10 ms delay
		ST7735_MADCTL , 1      ,  //	5: Memory access ctrl (directions), 1 arg:
			0x08,                   //		 Row addr/col addr, bottom to top refresh
		ST7735_DISSET5, 2      ,    //	6: Display settings #5, 2 args, no delay:
			0x15,                   //		 1 clk cycle nonoverlap, 2 cycle gate
															//		 rise, 3 cycle osc equalize
			0x02,                   //		 Fix on VTL
		ST7735_INVCTR , 1      ,    //	7: Display inversion control, 1 arg:
			0x0,                    //		 Line inversion
		ST7735_PWCTR1 , 2+DELAY,    //	8: Power control, 2 args + delay:
			0x02,                   //		 GVDD = 4.7V
			0x70,                   //		 1.0uA
			10,                     //		 10 ms delay
		ST7735_PWCTR2 , 1      ,    //	9: Power control, 1 arg, no delay:
			0x05,                   //		 VGH = 14.7V, VGL = -7.35V
		ST7735_PWCTR3 , 2      ,    // 10: Power control, 2 args, no delay:
			0x01,                   //		 Opamp current small
			0x02,                   //		 Boost frequency
		ST7735_VMCTR1 , 2+DELAY,    // 11: Power control, 2 args + delay:
			0x3C,                   //		 VCOMH = 4V
			0x38,                   //		 VCOML = -1.1V
			10,                     //		 10 ms delay
		ST7735_PWCTR6 , 2      ,    // 12: Power control, 2 args, no delay:
			0x11, 0x15,
		ST7735_GMCTRP1,16      ,    // 13: Magical unicorn dust, 16 args, no delay:
			0x09, 0x16, 0x09, 0x20, //		 (seriously though, not sure what
			0x21, 0x1B, 0x13, 0x19, //			these config values represent)
			0x17, 0x15, 0x1E, 0x2B,
			0x04, 0x05, 0x02, 0x0E,
		ST7735_GMCTRN1,16+DELAY,    // 14: Sparkles and rainbows, 16 args + delay:
			0x0B, 0x14, 0x08, 0x1E, //		 (ditto)
			0x22, 0x1D, 0x18, 0x1E,
			0x1B, 0x1A, 0x24, 0x2B,
			0x06, 0x06, 0x02, 0x0F,
			10,                     //		 10 ms delay
		ST7735_CASET  , 4      ,    // 15: Column addr set, 4 args, no delay:
			0x00, 0x02,             //		 XSTART = 2
			0x00, 0x81,             //		 XEND = 129
		ST7735_RASET  , 4      ,    // 16: Row addr set, 4 args, no delay:
			0x00, 0x02,             //		 XSTART = 1
			0x00, 0x81,             //		 XEND = 160
		ST7735_NORON  ,   DELAY,    // 17: Normal display on, no args, w/delay
			10,                     //		 10 ms delay
		ST7735_DISPON ,   DELAY,    // 18: Main screen turn on, no args, w/delay
			255 },                  //		 255 = 500 ms delay

	Rcmd1[] = {								 // Init for 7735R, part 1 (red or green tab)
		15,                         // 15 commands in list:
		ST7735_SWRESET,   DELAY,    //	1: Software reset, 0 args, w/delay
			150,                    //		 150 ms delay
		ST7735_SLPOUT ,   DELAY,    //	2: Out of sleep mode, 0 args, w/delay
			255,                    //		 500 ms delay
		ST7735_FRMCTR1, 3      ,    //	3: Frame rate ctrl - normal mode, 3 args:
			0x01, 0x2C, 0x2D,       //		 Rate = fosc/(1x2+40) * (LINE+2C+2D)
		ST7735_FRMCTR2, 3      ,    //	4: Frame rate control - idle mode, 3 args:
			0x01, 0x2C, 0x2D,       //		 Rate = fosc/(1x2+40) * (LINE+2C+2D)
		ST7735_FRMCTR3, 6      ,    //	5: Frame rate ctrl - partial mode, 6 args:
			0x01, 0x2C, 0x2D,       //		 Dot inversion mode
			0x01, 0x2C, 0x2D,       //		 Line inversion mode
		ST7735_INVCTR , 1      ,    //	6: Display inversion ctrl, 1 arg, no delay:
			0x07,                   //		 No inversion
		ST7735_PWCTR1 , 3      ,    //	7: Power control, 3 args, no delay:
			0xA2,
			0x02,                   //		 -4.6V
			0x84,                   //		 AUTO mode
		ST7735_PWCTR2 , 1      ,    //	8: Power control, 1 arg, no delay:
			0xC5,                   //		 VGH25 = 2.4C VGSEL = -10 VGH = 3 * AVDD
		ST7735_PWCTR3 , 2      ,    //	9: Power control, 2 args, no delay:
			0x0A,                   //		 Opamp current small
			0x00,                   //		 Boost frequency
		ST7735_PWCTR4 , 2      ,    // 10: Power control, 2 args, no delay:
			0x8A,                   //		 BCLK/2, Opamp current small & Medium low
			0x2A,  
		ST7735_PWCTR5 , 2      ,    // 11: Power control, 2 args, no delay:
			0x8A, 0xEE,
		ST7735_VMCTR1 , 1      ,    // 12: Power control, 1 arg, no delay:
			0x0E,
		ST7735_INVOFF , 0      ,    // 13: Don't invert display, no args, no delay
		ST7735_MADCTL , 1      ,    // 14: Memory access control (directions), 1 arg:
			0xC8,                   //		 row addr/col addr, bottom to top refresh
		ST7735_COLMOD , 1      ,    // 15: set color mode, 1 arg, no delay:
			0x05 },                 //		 16-bit color

	Rcmd2green[] = {						// Init for 7735R, part 2 (green tab only)
		2,                          //	2 commands in list:
		ST7735_CASET  , 4      ,    //	1: Column addr set, 4 args, no delay:
			0x00, 0x02,             //		 XSTART = 0
			0x00, 0x7F+0x02,        //		 XEND = 127
		ST7735_RASET  , 4      ,    //	2: Row addr set, 4 args, no delay:
			0x00, 0x01,             //		 XSTART = 0
			0x00, 0x9F+0x01 },      //		 XEND = 159
	Rcmd2red[] = {							// Init for 7735R, part 2 (red tab only)
		2,                          //	2 commands in list:
		ST7735_CASET  , 4      ,    //	1: Column addr set, 4 args, no delay:
			0x00, 0x00,             //		 XSTART = 0
			0x00, 0x7F,             //		 XEND = 127
		ST7735_RASET  , 4      ,    //	2: Row addr set, 4 args, no delay:
			0x00, 0x00,             //		 XSTART = 0
			0x00, 0x9F },           //		 XEND = 159

	Rcmd3[] = {								 // Init for 7735R, part 3 (red or green tab)
		4,                          //	4 commands in list:
		ST7735_GMCTRP1, 16      ,   //	1: Magical unicorn dust, 16 args, no delay:
			0x02, 0x1c, 0x07, 0x12,
			0x37, 0x32, 0x29, 0x2d,
			0x29, 0x25, 0x2B, 0x39,
			0x00, 0x01, 0x03, 0x10,
		ST7735_GMCTRN1, 16      ,   //	2: Sparkles and rainbows, 16 args, no delay:
			0x03, 0x1d, 0x07, 0x06,
			0x2E, 0x2C, 0x29, 0x2D,
			0x2E, 0x2E, 0x37, 0x3F,
			0x00, 0x00, 0x02, 0x10,
		ST7735_NORON  ,    DELAY,   //	3: Normal display on, no args, w/delay
			10,                     //		 10 ms delay
		ST7735_DISPON ,    DELAY,   //	4: Main screen turn on, no args w/delay
			100 };                  //  100 ms delay


// Companion code to the above tables.	Reads and issues
// a series of LCD commands stored in PROGMEM byte array.
void Teensy3_ST7735::commandList(uint8_t *addr)
{
	uint8_t	numCommands, numArgs;
	uint16_t ms;

	CS_ON();

	numCommands = *addr++;
	while (numCommands--)
	{
		SPICMD(*addr++);

		numArgs = *addr++;
		ms = numArgs & DELAY;
		numArgs &= ~DELAY;

		if (numArgs)
		{
			while (numArgs--)
			{
				uint8_t c = *addr++;
				SPIDATA1(c);
			}
		}

		if (ms)
		{
			ms = *addr++;
			if (ms == 255)
				ms = 500;
			delay(ms/4);
		}
	}

	CS_OFF();
}


struct CSPin
{
	uint8_t pin;
	uint8_t cs;
	volatile uint32_t *config;
};

const CSPin CSPins[] =
{
	{  2, 0, &CORE_PIN2_CONFIG  },
	{  6, 1, &CORE_PIN6_CONFIG  },
	{  9, 1, &CORE_PIN9_CONFIG  },
	{ 10, 0, &CORE_PIN10_CONFIG },
	{ 15, 4, &CORE_PIN15_CONFIG },
	{ 20, 2, &CORE_PIN20_CONFIG },
	{ 21, 3, &CORE_PIN21_CONFIG },
	{ 22, 3, &CORE_PIN22_CONFIG },
	{ 23, 2, &CORE_PIN23_CONFIG },
	{  0, }
};

static bool s_SPIInitDone = false;

// Initialization code common to both 'B' and 'R' type displays
bool Teensy3_ST7735::commonInit(uint8_t *cmdList)
{
	if (!s_SPIInitDone)
	{
		SPI.begin();
		SPI.setBitOrder(MSBFIRST);
		SPI.setDataMode(SPI_MODE0);
		s_SPIInitDone = true;
	}

	for (const CSPin *csp = CSPins; csp->pin; csp++)
	{
		if (csp->pin == _rs)
		{
			cmdcs_mask |= 1 << csp->cs;
			pinMode(csp->pin, OUTPUT);
			*(csp->config) = PORT_PCR_DSE | PORT_PCR_MUX(2);
			break;
		}
	}
	
	// SPI has already set these up, so we have to move them, if needed
	if (_sclk == 14)
	{
		pinMode(13, INPUT);
		CORE_PIN13_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(1);
	
		pinMode(14, OUTPUT);
		CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
	}

	if (_sid == 7)
	{
		pinMode(11, INPUT);
		CORE_PIN11_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(1);
	
		pinMode(7, OUTPUT);
		CORE_PIN7_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
	}

	// Set the master chip select lines to be "normally high" (ie active low)
	SPI0_MCR |= SPI_MCR_PCSIS(cmdcs_mask);

	mcr = SPI0_MCR;

	// Use both CTARs, allows for 16 bit transfer
	ctar0 = SPI_CTAR_FMSZ(7) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(BAUD_DIV) | SPI_CTAR_CSSCK(BAUD_DIV) | SPI_CTAR_DBR;
	ctar1 = SPI_CTAR_FMSZ(15) | SPI_CTAR_PBR(0) | SPI_CTAR_BR(BAUD_DIV) | SPI_CTAR_CSSCK(BAUD_DIV) | SPI_CTAR_DBR;
	
	SPI0_CTAR0 = ctar0;
	SPI0_CTAR1 = ctar1;

	digitalWrite(_cs, HIGH);
	pinMode(_cs, OUTPUT);

	// toggle RST low to reset
	if (_rst)
	{
		digitalWrite(_rst, HIGH);
		pinMode(_rst, OUTPUT);
		delay(1);
		digitalWrite(_rst, LOW);
		delay(1);
		digitalWrite(_rst, HIGH);
		delay(1);
	}

	if (cmdList)
		commandList(cmdList);

	return true;
}


// Initialization for ST7735B screens
bool Teensy3_ST7735::initB()
{
	return commonInit(Bcmd);
}

// Initialization for ST7735R screens (green or red tabs)
bool Teensy3_ST7735::initR(uint8_t options)
{
	if (commonInit(Rcmd1))
	{
		if (options == INITR_GREENTAB)
		{
			commandList(Rcmd2green);
			colstart = 2;
			rowstart = 1;
		}
		else
			commandList(Rcmd2red); // colstart, rowstart left at default '0' values

        // if black, change MADCTL color filter
        if (options == INITR_BLACKTAB) {
            SPICMD(ST7735_MADCTL);
            SPIDATA1(0xC0);
        }

		commandList(Rcmd3);
        tabcolor = options;

		return true;
	}

	return false;
}

void Teensy3_ST7735::setAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
	SETADDRWINDOW(x0,y0,x1,y1);
}

void Teensy3_ST7735::fillScreen(uint16_t color)
{
	CS_ON();

	SETADDRWINDOW(0, 0, _width-1, _height-1);

	for (int num = _width*_height; num > 0; num--)
		SPIDATA2(color);

	SPIWAIT();
	CS_OFF();
}


void Teensy3_ST7735::pushColor(uint16_t color)
{
	CS_ON();

    if (tabcolor == INITR_BLACKTAB)   color = swapcolor(color);
	SPIDATA2(color);

	SPIWAIT();
	CS_OFF();
}

void Teensy3_ST7735::_drawPixel(int16_t x, int16_t y, uint16_t color)
{
	if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

	SETADDRWINDOW(x,y,x+1,y+1);

	SPIDATA2(color);
}

void Teensy3_ST7735::drawPixel(int16_t x, int16_t y, uint16_t color)
{
	CS_ON();

	if((x < 0) ||(x >= _width) || (y < 0) || (y >= _height)) return;

	SETADDRWINDOW(x,y,x+1,y+1);

	SPIDATA2(color);

	SPIWAIT();
	CS_OFF();
}


void Teensy3_ST7735::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
	CS_ON();

	if((x >= _width) || (y >= _height)) return;
	if((y+h-1) >= _height) h = _height-y;
	SETADDRWINDOW(x, y, x, y+h-1);

    if (tabcolor == INITR_BLACKTAB)   color = swapcolor(color);
	while (h--)
		SPIDATA2(color);

	SPIWAIT();
	CS_OFF();
}


void Teensy3_ST7735::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
	CS_ON();

	// Rudimentary clipping
	if((x >= _width) || (y >= _height)) return;
	if((x+w-1) >= _width)	w = _width-x;
	SETADDRWINDOW(x, y, x+w-1, y);

    if (tabcolor == INITR_BLACKTAB)   color = swapcolor(color);
	while (w--)
		SPIDATA2(color);

	SPIWAIT();
	CS_OFF();
}


// fill a rectangle
void Teensy3_ST7735::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
	CS_ON();

	// rudimentary clipping (drawChar w/big text requires this)
	if((x >= _width) || (y >= _height)) return;
	if((x + w - 1) >= _width)	w = _width	- x;
	if((y + h - 1) >= _height) h = _height - y;

	SETADDRWINDOW(x, y, x+w-1, y+h-1);

	for (int i=w*h; i > 0; i--)
		SPIDATA2(color);

	SPIWAIT();
	CS_OFF();
}


// Pass 8-bit (each) R,G,B, get back 16-bit packed color
uint16_t Teensy3_ST7735::Color565(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x08
#define MADCTL_MH  0x04

void Teensy3_ST7735::setRotation(uint8_t m)
{
	CS_ON();

	madctl = MADCTL_RGB;

	SPICMD(ST7735_MADCTL);

	rotation = m % 4; // can't be higher than 3
	
	switch (rotation)
	{
		case 0:
			madctl |= MADCTL_MX | MADCTL_MY;
			_width  = ST7735_TFTWIDTH;
			_height = ST7735_TFTHEIGHT;
		break;
		case 1:
			madctl |= MADCTL_MY | MADCTL_MV;
			_width  = ST7735_TFTHEIGHT;
			_height = ST7735_TFTWIDTH;
		break;
		case 2:
			madctl |= 0;
			_width  = ST7735_TFTWIDTH;
			_height = ST7735_TFTHEIGHT;
		break;
		case 3:
			madctl |= MADCTL_MX | MADCTL_MV;
			_width  = ST7735_TFTHEIGHT;
			_height = ST7735_TFTWIDTH;
		break;
	}

	if (madctl & MADCTL_MX)
		madctl |= MADCTL_MH;
	
	if (madctl & MADCTL_MY)
		madctl |= MADCTL_ML;

	SPIDATA1(madctl);

	SPIWAIT();
	CS_OFF();
}


void Teensy3_ST7735::invertDisplay(boolean i) {
	SPICMD(i ? ST7735_INVON : ST7735_INVOFF);
}

void Teensy3_ST7735::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size)
{
	CS_ON();

	if (size == 1 && bg != color)
	{
		if ((x >= _width) || (y >= _height) || ((x + 5-1) < 0) || ((y + 8-1) < 0))
			return;

		SPICMD(ST7735_MADCTL);
		SPIDATA1(madctl ^ MADCTL_MV);

		//SETADDRWINDOW(x, y, x+6-1, y+8-1);
		SETADDRWINDOW(y, x, y+8-1, x+6-1);
		uint16_t col[2] = { bg, color };

		uint8_t *ptr = &font[c*5];

		for (x=0; x < 5; x++)
		{
			uint8_t line = *ptr++;

			for (y=0; y < 8; y++)
			{
				SPIDATA2(col[line&1]);
				line>>=1;
			}
		}

		for (y=0; y < 8; y++)
			SPIDATA2(bg);

		SPICMD(ST7735_MADCTL);
		SPIDATA1(madctl);

		SPIWAIT();
		CS_OFF();
	}
	else
		_drawChar(x, y, c, color, bg, size);
}






///////////////





// draw a circle outline
void Teensy3_ST7735::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  int16_t f = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x = 0;
  int16_t y = r;

  _drawPixel(x0, y0+r, color);
  _drawPixel(x0, y0-r, color);
  _drawPixel(x0+r, y0, color);
  _drawPixel(x0-r, y0, color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
  
    _drawPixel(x0 + x, y0 + y, color);
    _drawPixel(x0 - x, y0 + y, color);
    _drawPixel(x0 + x, y0 - y, color);
    _drawPixel(x0 - x, y0 - y, color);
    _drawPixel(x0 + y, y0 + x, color);
    _drawPixel(x0 - y, y0 + x, color);
    _drawPixel(x0 + y, y0 - x, color);
    _drawPixel(x0 - y, y0 - x, color);
    
  }
}

void Teensy3_ST7735::drawCircleHelper( int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color) {
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      _drawPixel(x0 + x, y0 + y, color);
      _drawPixel(x0 + y, y0 + x, color);
    } 
    if (cornername & 0x2) {
      _drawPixel(x0 + x, y0 - y, color);
      _drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      _drawPixel(x0 - y, y0 + x, color);
      _drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      _drawPixel(x0 - y, y0 - x, color);
      _drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void Teensy3_ST7735::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// used to do circles and roundrects!
void Teensy3_ST7735::fillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
  int16_t f     = 1 - r;
  int16_t ddF_x = 1;
  int16_t ddF_y = -2 * r;
  int16_t x     = 0;
  int16_t y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}



#define swap(a, b) { int16_t t = a; a = b; b = t; }

void Teensy3_ST7735::drawLine(int16_t x0, int16_t y0,  int16_t x1, int16_t y1, uint16_t color)
{
	CS_ON();

	rowstart = colstart = 0;

	int16_t steep = abs(y1 - y0) > abs(x1 - x0);

	if (steep)
	{
		swap(x0, y0);
		swap(x1, y1);
	}

	if (x0 > x1)
	{
		swap(x0, x1);
		swap(y0, y1);
	}
	
	int ystep = (y0 > y1) ? -1 : 1;

	int16_t dx = x1-x0;
	int16_t dy = abs(y1-y0);

	int16_t err = dx / 2;

	if (steep)
		SETADDRWINDOW(y0,x0,y0,_height);
	else
		SETADDRWINDOW(x0,y0,_width,y0);

	while (x0 <= x1)
	{
		SPIDATA2(color);

		x0++;

		err -= dy;
		if (err < 0)
		{
			y0 += ystep;
			err += dx;

			if (steep)
				SETADDRWINDOW(y0,x0,y0,_height);
			else
				SETADDRWINDOW(x0,y0,_width,y0);
		}
	}

	SPIWAIT();
	CS_OFF();
}


// draw a rectangle
void Teensy3_ST7735::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
	drawFastHLine(x, y, w, color);
	drawFastHLine(x, y+h-1, w, color);
	drawFastVLine(x, y, h, color);
	drawFastVLine(x+w-1, y, h, color);
}

// draw a rounded rectangle!
void Teensy3_ST7735::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
  // smarter version
  drawFastHLine(x+r  , y    , w-2*r, color); // Top
  drawFastHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawFastVLine(  x    , y+r  , h-2*r, color); // Left
  drawFastVLine(  x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// fill a rounded rectangle!
void Teensy3_ST7735::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}

// draw a triangle!
void Teensy3_ST7735::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

// fill a triangle!
void Teensy3_ST7735::fillTriangle ( int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
  int16_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    drawFastHLine(a, y0, b-a+1, color);
    return;
  }

  int16_t
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1,
    sa   = 0,
    sb   = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }
}

void Teensy3_ST7735::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color)
{
	int16_t i, j, byteWidth = (w + 7) / 8;

	for(j=0; j<h; j++)
	{
		for(i=0; i<w; i++)
		{
			if (bitmap[j * byteWidth + i / 8] & (128 >> (i & 7)))
				_drawPixel(x+i, y+j, color);
		}
	}
}


size_t Teensy3_ST7735::write(uint8_t c)
{
	if (c == '\n')
	{
		cursor_y += textsize*8;
		cursor_x = 0;
	}
	else if (c == '\r')
	{
		// skip em
	}
	else
	{
		drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
		cursor_x += textsize*6;
		
		if (wrap && (cursor_x > (_width - textsize*6)))
		{
			cursor_y += textsize*8;
			cursor_x = 0;
		}
	}

	return 1;
}

// draw a character
void Teensy3_ST7735::_drawChar(int16_t x, int16_t y, unsigned char c,
			    uint16_t color, uint16_t bg, uint8_t size) {

  if((x >= _width)            || // Clip right
     (y >= _height)           || // Clip bottom
     ((x + 5 * size - 1) < 0) || // Clip left
     ((y + 8 * size - 1) < 0))   // Clip top
    return;

  for (int8_t i=0; i<6; i++ ) {
    uint8_t line;
    if (i == 5) 
      line = 0x0;
    else 
      line = pgm_read_byte(font+(c*5)+i);
    for (int8_t j = 0; j<8; j++) {
      if (line & 0x1) {
        if (size == 1) // default size
          _drawPixel(x+i, y+j, color);
        else {  // big size
          fillRect(x+(i*size), y+(j*size), size, size, color);
        } 
      } else if (bg != color) {
        if (size == 1) // default size
          _drawPixel(x+i, y+j, bg);
        else {  // big size
          fillRect(x+i*size, y+j*size, size, size, bg);
        } 	
      }
      line >>= 1;
    }
  }
}

void Teensy3_ST7735::setCursor(int16_t x, int16_t y)
{
	cursor_x = x;
	cursor_y = y;
}


void Teensy3_ST7735::setTextSize(uint8_t s)
{
	textsize = (s > 0) ? s : 1;
}


void Teensy3_ST7735::setTextColor(uint16_t c)
{
    if (tabcolor == INITR_BLACKTAB)   c = swapcolor(c);
	textcolor = c;
	textbgcolor = c; 
	// for 'transparent' background, we'll set the bg 
	// to the same as fg instead of using a flag
}

void Teensy3_ST7735::setTextColor(uint16_t c, uint16_t b)
{
    if (tabcolor == INITR_BLACKTAB)   c = swapcolor(c);
    if (tabcolor == INITR_BLACKTAB)   b = swapcolor(b);
	textcolor = c;
	textbgcolor = b; 
}

void Teensy3_ST7735::setTextWrap(boolean w)
{
	wrap = w;
}

uint8_t Teensy3_ST7735::getRotation(void)
{
	rotation %= 4;
	return rotation;
}

// return the size of the display which depends on the rotation!
int16_t Teensy3_ST7735::width(void)
{
	return _width; 
}
 
int16_t Teensy3_ST7735::height(void)
{
	return _height; 
}
