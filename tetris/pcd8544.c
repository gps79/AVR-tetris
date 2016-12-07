/*
 *
 * Name         :  pcd8544.c
 *
 * Description  :  This is a driver for the PCD8544 graphic LCD.
 *                 Based on the code written by Sylvain Bissonette
 *                 This driver is buffered in 504 bytes memory be sure
 *                 that your MCU having bigger memory
 *
 * Author       :  Fandi Gunawan <fandigunawan@gmail.com>
 * Website      :  http://fandigunawan.wordpress.com
 *
 * Credit       :  Sylvain Bissonette (2003)
 *
 * License      :  GPL v. 3
 *
 * Compiler     :  WinAVR, GCC for AVR platform
 *                 Tested version :
 *                 - 20070525 (avr-libc 1.4)
 *                 - 20071221 (avr-libc 1.6)
 *                 - 20081225 tested by Jakub Lasinski
 *                 - other version please contact me if you find out it is working
 * Compiler note:  Please be aware of using older/newer version since WinAVR
 *                 is under extensive development. Please compile with parameter -O1
 *
 * History      :
 * Version 0.2.6 (March 14, 2009) additional optimization by Jakub Lasinski
 * + Optimization using Memset and Memcpy
 * + Bug fix and sample program reviewed
 * + Commented <stdio.h>
 * Version 0.2.5 (December 25, 2008) x-mas version :)
 * + Boundary check is added (reported by starlino on Dec 20, 2008)
 * + Return value is added, it will definitely useful for error checking
 * Version 0.2.4 (March 5, 2008)
 * + Multiplier was added to LcdBars to scale the bars
 * Version 0.2.3 (February 29, 2008)
 * + Rolled back LcdFStr function because of serious bug
 * + Stable release
 * Version 0.2.2 (February 27, 2008)
 * + Optimizing LcdFStr function
 * Version 0.2.1 (January 2, 2008)
 * + Clean up codes
 * + All settings were migrated to pcd8544.h
 * + Using _BV() instead of << to make a better readable code
 * Version 0.2 (December 11-14, 2007)
 * + Bug fixed in LcdLine() and LcdStr()
 * + Adding new routine
 *    - LcdFStr()
 *    - LcdSingleBar()
 *    - LcdBars()
 *    - LcdRect()
 *    - LcdImage()
 * + PROGMEM used instead of using.data section
 * Version 0.1 (December 3, 2007)
 * + First stable driver
 *
 * Note         :
 * Font will be displayed this way (16x6)
 * 1 2 3 4 5 6 7 8 9 0 1 2 3 4
 * 2
 * 3
 * 4
 * 5
 * 6
 *
 * Contributor : 
 * + Jakub Lasinski
 */

 #define __ASSERT_USE_STDERR
 //#define NDEBUG

#include <avr/io.h>
#include <string.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "my_assert.h"
#include "pcd8544.h"

/* Function prototypes */

static void LcdSend    ( byte data, LcdCmdData cd );

/* Global variables */

/* Cache buffer in SRAM 84*48 bits or 504 bytes */
byte LcdCache [ LCD_CACHE_SIZE ];

/* Cache index */
static int   LcdCacheIdx;
static LcdPixelMode g_drawingPen = PIXEL_ON;

/*
 * Name         :  LcdInit
 * Description  :  Performs MCU SPI & LCD controller initialization.
 * Argument(s)  :  None.
 * Return value :  None.
 */
void LcdInit ( void )
{
    /* Pull-up on reset pin. */
//    LCD_PORT |= _BV ( LCD_RST_PIN ); // not needed ?? (works without this line)

    /* Set output bits on LCD Port. */
    LCD_DDR |= _BV( LCD_RST_PIN ) | _BV( LCD_DC_PIN ) | _BV( LCD_CE_PIN ) | _BV( SPI_MOSI_PIN ) | _BV( SPI_CLK_PIN );

//    Delay(); // not needed ?? (works without this line)

    /* Toggle display reset pin. */
//    LCD_PORT &= ~( _BV( LCD_RST_PIN ) ); // not needed ?? (works without this line)
//    Delay(); // not needed ?? (works without this line)
    LCD_PORT |= _BV ( LCD_RST_PIN );

    /* Enable SPI port:
    * No interrupt, MSBit first, Master mode, CPOL->0, CPHA->0, Clk/4
    */
    SPCR = 0x50;

    /* Disable LCD controller */
//    LCD_PORT |= _BV( LCD_CE_PIN ); // not needed ?? (works without this line)

//    LcdSend( 0x21, LCD_CMD ); /* LCD Extended Commands. */  // not needed ?? (works without this line)
//    LcdSend( 0xC8, LCD_CMD ); /* Set LCD Vop (Contrast).*/ // not needed ?? (works without this line)
//    LcdSend( 0x06, LCD_CMD ); /* Set Temp coefficent. */ // not needed ?? (works without this line)
//    LcdSend( 0x13, LCD_CMD ); /* LCD bias mode 1:48. */ // not needed ?? (works without this line)
    LcdSend( 0x20, LCD_CMD ); /* LCD Standard Commands,Horizontal addressing mode */
    LcdSend( 0x0C, LCD_CMD ); /* LCD in normal mode. */
}

/*
 * Name         :  LcdClear
 * Description  :  Clears the display. LcdUpdate must be called next.
 * Argument(s)  :  None.
 * Return value :  None.
 * Note         :  Based on Sylvain Bissonette's code
 */
void LcdClear ( void )
{
	memset(LcdCache,0x00,LCD_CACHE_SIZE);
}

/*
 * Name         :  LcdGotoXYFont
 * Description  :  Sets cursor location to xy location corresponding to basic
 *                 font size.
 * Argument(s)  :  x, y -> Coordinate for new cursor position. Range: 1,1 .. 14,6
 * Return value :  see return value in pcd8544.h
 * Note         :  Based on Sylvain Bissonette's code
 */
byte LcdGotoXYFont ( byte x, byte y )
{
    /* Boundary check, slow down the speed but will guarantee this code wont fail */
    /* Version 0.2.5 - Fixed on Dec 25, 2008 (XMAS) */
    if( x > 14)
        return OUT_OF_BORDER;
    if( y > 6)
        return OUT_OF_BORDER;
    /*  Calculate index. It is defined as address within 504 bytes memory */

    LcdCacheIdx = ( x - 1 ) * 6 + ( y - 1 ) * 84;
    return OK;
}

/*
 * Name         :  LcdChr
 * Description  :  Displays a character at current cursor location and
 *                 increment cursor location.
 * Argument(s)  :  size -> Font size. See enum in pcd8544.h.
 *                 ch   -> Character to write.
 * Return value :  see pcd8544.h about return value
 */
byte LcdChr ( LcdFontSize size, byte ch )
{
    byte i, c;
    byte b1, b2;
    int  tmpIdx;

    if ( (ch < 0x20) || (ch > 0x7b) )
    {
        /* Convert to a printable character. */
        ch = 92;
    }

    if ( size == FONT_1X )
    {
        for ( i = 0; i < 5; i++ )
        {
            /* Copy lookup table from Flash ROM to LcdCache */
            LcdCache[LcdCacheIdx++] = pgm_read_byte(&( FontLookup[ ch - 32 ][ i ] ) ) << 1;
        }
    }
    else if ( size == FONT_2X )
    {
        tmpIdx = LcdCacheIdx - 84;

        if ( tmpIdx < 0 ) return OUT_OF_BORDER;

        for ( i = 0; i < 5; i++ )
        {
            /* Copy lookup table from Flash ROM to temporary c */
            c = pgm_read_byte(&(FontLookup[ch - 32][i])) << 1;
            /* Enlarge image */
            /* First part */
            b1 =  (c & 0x01) * 3;
            b1 |= (c & 0x02) * 6;
            b1 |= (c & 0x04) * 12;
            b1 |= (c & 0x08) * 24;

            c >>= 4;
            /* Second part */
            b2 =  (c & 0x01) * 3;
            b2 |= (c & 0x02) * 6;
            b2 |= (c & 0x04) * 12;
            b2 |= (c & 0x08) * 24;

            /* Copy two parts into LcdCache */
            LcdCache[tmpIdx++] = b1;
            LcdCache[tmpIdx++] = b1;
            LcdCache[tmpIdx + 82] = b2;
            LcdCache[tmpIdx + 83] = b2;
        }

        /* Update x cursor position. */
        /* Version 0.2.5 - Possible bug fixed on Dec 25,2008 */
        LcdCacheIdx = (LcdCacheIdx + 11) % LCD_CACHE_SIZE;
    }

    /* Horizontal gap between characters. */
    /* Version 0.2.5 - Possible bug fixed on Dec 25,2008 */
    LcdCache[LcdCacheIdx] = 0x00;
    /* At index number LCD_CACHE_SIZE - 1, wrap to 0 */
    if(LcdCacheIdx == (LCD_CACHE_SIZE - 1) )
    {
        LcdCacheIdx = 0;
        return OK_WITH_WRAP;
    }
    /* Otherwise just increment the index */
    LcdCacheIdx++;
    return OK;
}

/*
 * Name         :  LcdStr
 * Description  :  Displays a character at current cursor location and increment
 *                 cursor location according to font size. This function is
 *                 dedicated to print string laid in SRAM
 * Argument(s)  :  size      -> Font size. See enum.
 *                 dataArray -> Array contained string of char to be written
 *                              into cache.
 * Return value :  see return value on pcd8544.h
 */
byte LcdStr ( LcdFontSize size, byte dataArray[] )
{
    byte tmpIdx=0;
    byte response;
    while( dataArray[ tmpIdx ] != '\0' )
	{
        /* Send char */
		response = LcdChr( size, dataArray[ tmpIdx ] );
        /* Just in case OUT_OF_BORDER occured */
        /* Dont worry if the signal == OK_WITH_WRAP, the string will
        be wrapped to starting point */
        if( response == OUT_OF_BORDER)
            return OUT_OF_BORDER;
        /* Increase index */
		tmpIdx++;
	}
    return OK;
}

/*
 * Name         :  LcdFStr
 * Description  :  Displays a characters at current cursor location and increment
 *                 cursor location according to font size. This function is
 *                 dedicated to print string laid in Flash ROM
 * Argument(s)  :  size    -> Font size. See enum.
 *                 dataPtr -> Pointer contained string of char to be written
 *                            into cache.
 * Return value :  see return value on pcd8544.h
 * Example      :  LcdFStr(FONT_1X, PSTR("Hello World"));
 *                 LcdFStr(FONT_1X, &name_of_string_as_array);
 */
byte LcdFStr ( LcdFontSize size, const byte *dataPtr )
{
    byte c;
    byte response;
    for ( c = pgm_read_byte( dataPtr ); c; ++dataPtr, c = pgm_read_byte( dataPtr ) )
    {
        /* Put char */
        response = LcdChr( size, c );
        if(response == OUT_OF_BORDER)
            return OUT_OF_BORDER;
    }
	/* Fixed by Jakub Lasinski. Version 0.2.6, March 14, 2009 */
    return OK;
}

void LcdSetPixel ( byte x, byte y )
{
	word  index;
	byte  bitMask;

	assert( x < LCD_X_RES );
	assert( y < LCD_Y_RES );

	index = ( ( y >> 3 ) * 84 ) + x;
	bitMask = 0x01 << (y & 0x07);
	uint8_t *addr = &LcdCache[ index ];
	uint8_t value = *addr; // splitting LcdCache[ index ] |= bitMask;  helps compiler to optimize (saved 2B)
	if (PIXEL_ON == g_drawingPen)
		value |= bitMask;
	else
		value &= ( ~bitMask);
	*addr = value;
}

void LcdSetPen ( LcdPixelMode pen )
{
	assert((pen==PIXEL_OFF) || (pen==PIXEL_ON));
	g_drawingPen = pen;
}

/*
 * Name         :  LcdBar
 * Description  :  Display single bar.
 * Argument(s)  :  baseX  -> absolute x axis coordinate
 *                 baseY  -> absolute y axis coordinate
 *				   width  -> width of bar (in pixel)
 *				   height -> height of bar (in pixel)
 */
void LcdBar ( byte baseX, byte baseY, byte width, byte height)
{
	assert(baseX < LCD_X_RES);
	assert(baseY < LCD_Y_RES);
	assert(baseX+width <= LCD_X_RES);
	assert(baseY+height <= LCD_Y_RES);

	while (height)
	{
		byte x = baseX;
		byte xCounter = width;
		while (xCounter)
		{
			LcdSetPixel( x, baseY);
			++x;
			--xCounter;
		}
		++baseY;
		--height;
	}
}

void LcdUpdate ( void )
{
	// Set base address to position (0,0).
	LcdSend( 0x80, LCD_CMD );
	LcdSend( 0x40, LCD_CMD );

	// 504 bytes to send. we split it for two loops: 255 and 249 iterations
	byte *byteToSend = LcdCache;
	uint16_t i = 504;
	while (i)
	{
		LcdSend( *byteToSend, LCD_DATA );
		--i;
		byteToSend++;
	}
}

/*
 * Name         :  LcdSend
 * Description  :  Sends data to display controller.
 * Argument(s)  :  data -> Data to be sent
 *                 cd   -> Command or data (see enum in pcd8544.h)
 * Return value :  None.
 */
static void LcdSend ( byte data, LcdCmdData cd )
{
    /*  Enable display controller (active low). */
    LCD_PORT &= ~( _BV( LCD_CE_PIN ) );

    if ( cd == LCD_DATA )
    {
        LCD_PORT |= _BV( LCD_DC_PIN );
    }
    else
    {
        LCD_PORT &= ~( _BV( LCD_DC_PIN ) );
    }

    /*  Send data to display controller. */
    SPDR = data;

    /*  Wait until Tx register empty. */
    while ( (SPSR & 0x80) != 0x80 );


    /* Disable display controller. */
    LCD_PORT |= _BV( LCD_CE_PIN );
}


void __assert(const char *__file, int __lineno)
{
	LcdGotoXYFont(1,1);
	LcdFStr(FONT_1X,(unsigned char*)PSTR("Assert:"));
	LcdGotoXYFont(1,2);
	LcdStr(FONT_1X,(unsigned char*)(__file));
	LcdGotoXYFont(1,3);
	char str[15] = "Line:";
	itoa(__lineno, str+5, 10);
	LcdStr(FONT_1X,(unsigned char*)(str));
	LcdUpdate();
	while (1){}
}
