/*
 *
 * Name         :  pcd8544.c
 *
 * Description  :  This is a driver for the PCD8544 graphic LCD.
 *                 Based on the code written by Sylvain Bissonette
 *                 This driver is buffered in 504 bytes memory be sure
 *                 that your MCU having bigger memory
 *                 The driver has been re-designed for 1KB Challenge
 *
 * Author       :  Fandi Gunawan <fandigunawan@gmail.com>
 * Modified by  :  Grzegorz Pietrusiak <gpsspam2@gmail.com>
 * Website      :  http://fandigunawan.wordpress.com
 *
 * Credit       :  Sylvain Bissonette (2003)
 *
 * License      :  GPL v. 3
 *
 */

 
/* Global variables */

/* Cache buffer in SRAM 84*48 bits or 504 bytes */
uint8_t LcdCache [ LCD_CACHE_SIZE ];

/* Cache index */
#ifndef NDEBUG
static int   LcdCacheIdx;
#endif

/*
 * Name         :  LcdInit
 * Description  :  Performs MCU SPI & LCD controller initialization.
 * Argument(s)  :  None.
 * Return value :  None.
 */
static void LcdInit ( void ) // once static it will be built-in as inline
{
    /* Set output bits on LCD Port. */
    LCD_DDR |= _BV( LCD_RST_PIN ) | _BV( LCD_DC_PIN ) | _BV( LCD_CE_PIN ) | _BV( SPI_MOSI_PIN ) | _BV( SPI_CLK_PIN );
    /* Toggle display reset pin. */
    LCD_PORT |= _BV ( LCD_RST_PIN );

    // Enable SPI port: No interrupt, MSBit first, Master mode, CPOL->0, CPHA->0, Clk/4
    SPCR = 0x50;

	LCD_SET_COMMANDS_SENDING_MODE;
    LcdSend( 0x20 ); /* LCD Standard Commands,Horizontal addressing mode */
    LcdSend( 0x0C ); /* LCD in normal mode. */
	LCD_SET_DATA_SENDING_MODE; // from now on only data will be sent to the LCD
}

static void LcdUpdate ( void )
{
	uint8_t *byteToSend = LcdCache;
	uint16_t i = 504;
	while (i)
	{
		LcdSend( *byteToSend );
		--i;
		byteToSend++;
	}
}

/*
 * Name         :  LcdBar
 * Description  :  Display single bar.
 * Argument(s)  :  baseX  -> absolute x axis coordinate
 *                 baseY  -> absolute y axis coordinate
 *				   width  -> width of bar (in pixel)
 *				   height -> height of bar (in pixel)
 */
static void LcdBar ( uint8_t baseX, uint8_t baseY, uint8_t width, uint8_t height)
{
	assert(baseX < LCD_X_RES);
	assert(baseY < LCD_Y_RES);
	assert(baseX+width <= LCD_X_RES);
	assert(baseY+height <= LCD_Y_RES);

	bool mode = TRUE; // draw black pixels
	if (baseY&0x01) mode = FALSE; // draw blank pixels
	while (height)
	{
		uint8_t x = baseX;
		uint8_t xCounter = width;
		while (xCounter)
		{
			//LcdSetPixel( x, baseY);
			{
				uint16_t  index;
				uint8_t  bitMask;

				assert( x < LCD_X_RES );
				assert( baseY < LCD_Y_RES );

				index = ( ( baseY >> 3 ) * 84 ) + x;
				bitMask = 0x01 << (baseY & 0x07);
				uint8_t *addr = &LcdCache[ index ];
				uint8_t value = *addr; // splitting LcdCache[ index ] |= bitMask;  it helps the compiler to optimize (we saved 2B)
				if (mode)
				{
					value |= bitMask;
				}
				else
				{
					value &= ( ~bitMask);
				}
				*addr = value;			
			}
			++x;
			--xCounter;
		}
		++baseY;
		--height;
	}
}

/*
 * Name         :  LcdSend
 * Description  :  Sends data to display controller.
 * Argument(s)  :  data -> Data to be sent
 * Return value :  None.
 */

static void LcdSend ( uint8_t data )
{
	 // Send data and wait for the Tx register
	 SPDR = data;
	 while ( !(SPSR & 0x80) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// supporting functions; not used in "release"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef NDEBUG
/*
 * Name         :  LcdGotoXYFont
 * Description  :  Sets cursor location to xy location corresponding to basic
 *                 font size.
 * Argument(s)  :  x, y -> Coordinate for new cursor position. Range: 1,1 .. 14,6
 * Return value :  see return value in pcd8544.h
 * Note         :  Based on Sylvain Bissonette's code
 */
static uint8_t LcdGotoXYFont ( uint8_t x, uint8_t y )
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
static uint8_t LcdChr ( LcdFontSize size, uint8_t ch )
{
    uint8_t i, c;
    uint8_t b1, b2;
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
static uint8_t LcdStr ( LcdFontSize size, uint8_t dataArray[] )
{
    uint8_t tmpIdx=0;
    uint8_t response;
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
static uint8_t LcdFStr ( LcdFontSize size, const uint8_t *dataPtr )
{
    uint8_t c;
    uint8_t response;
    for ( c = pgm_read_byte( dataPtr ); c; ++dataPtr, c = pgm_read_byte( dataPtr ) )
    {
        /* Put char */
        response = LcdChr( size, c );
        if(response == OUT_OF_BORDER)
            return OUT_OF_BORDER;
    }
    return OK;
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

#endif // NDEBUG