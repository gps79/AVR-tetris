/*
 *
 * Name         :  pcd8544.h
 *
 * Description  :  This is header file for the PCD8544 graphic LCD driver.
 *                 Based on the code written by Sylvain Bissonette
 *
 * Author       :  Fandi Gunawan <fandigunawan@gmail.com>
 * Website      :  http://fandigunawan.wordpress.com
 *
 * Credit       :  Sylvain Bissonette (2003)
 *                 Louis Frigon (2003)
 *
 * License      :  GPL v. 3
 *
 * Compiler     :  WinAVR, GCC for AVR platform
 *                 Tested version :
 *                 - 20070525
 *                 - 20071221
 *                 - 20081225
 * Compiler note:  Please be aware of using older/newer version since WinAVR
 *                 is in extensive development. Please compile with parameter -O1
 *
 * History      :
 * Please refer to pcd8544.c
 */

#ifndef _PCD8544_H_
#define _PCD8544_H_

/* General constants for driver */
#define FALSE                      0
#define TRUE                       1

/* For return value */
#define OK                         0
#define OUT_OF_BORDER              1
#define OK_WITH_WRAP               2


#define LCD_X_RES                  84    /* x resolution */
#define LCD_Y_RES                  48    /* y resolution */
#define EMPTY_SPACE_BARS           2
#define BAR_X                      5
#define BAR_Y                      38

/* LCD Port */
#define LCD_PORT                   PORTB
#define LCD_DDR                    DDRB

typedef uint8_t bool;

/* ATMega8 port pinout for LCD. */
/* 0.2.6 bug, fixed */
#define LCD_DC_PIN                 PB0  /* Pin 0 */
#define LCD_CE_PIN                 PB2  /* Pin 2 */
#define SPI_MOSI_PIN               PB3  /* Pin 3 */
#define LCD_RST_PIN                PB4  /* Pin 4 */
#define SPI_CLK_PIN                PB5  /* Pin 5 */

/* Cache size in bytes ( 84 * 48 ) / 8 = 504 bytes */
#define LCD_CACHE_SIZE             ( ( LCD_X_RES * LCD_Y_RES ) / 8)
extern uint8_t LcdCache [ LCD_CACHE_SIZE ];

#define LCD_SET_DATA_SENDING_MODE (LCD_PORT |= _BV( LCD_DC_PIN ))
#define LCD_SET_COMMANDS_SENDING_MODE (LCD_PORT &= ~( _BV( LCD_DC_PIN ) ))

typedef enum
{
    PIXEL_OFF =  0,
    PIXEL_ON  =  1

} LcdPixelMode;

typedef enum
{
    FONT_1X = 1,
    FONT_2X = 2

} LcdFontSize;

/* Function prototypes */
#ifndef NDEBUG
static uint8_t LcdGotoXYFont ( uint8_t x, uint8_t y );
static uint8_t LcdStr        ( LcdFontSize size, uint8_t dataArray[] );
#endif
static void LcdBar          ( uint8_t baseX, uint8_t baseY, uint8_t height, uint8_t width);
static void LcdSend ( uint8_t data );


/*
 * Character lookup table code was taken from the work of Sylvain Bissonette
 * This table defines the standard ASCII characters in a 5x7 dot format.
 */
static const uint8_t FontLookup [][5] PROGMEM=
{
    { 0x00, 0x00, 0x00, 0x00, 0x00 },   /* space */
    { 0x00, 0x00, 0x2f, 0x00, 0x00 },   /* ! */
    { 0x00, 0x07, 0x00, 0x07, 0x00 },   /* " */
    { 0x14, 0x7f, 0x14, 0x7f, 0x14 },   /* # */
    { 0x24, 0x2a, 0x7f, 0x2a, 0x12 },   /* $ */
    { 0xc4, 0xc8, 0x10, 0x26, 0x46 },   /* % */
    { 0x36, 0x49, 0x55, 0x22, 0x50 },   /* & */
    { 0x00, 0x05, 0x03, 0x00, 0x00 },   /* ' */
    { 0x00, 0x1c, 0x22, 0x41, 0x00 },   /* ( */
    { 0x00, 0x41, 0x22, 0x1c, 0x00 },   /* ) */
    { 0x14, 0x08, 0x3E, 0x08, 0x14 },   /* * */
    { 0x08, 0x08, 0x3E, 0x08, 0x08 },   /* + */
    { 0x00, 0x00, 0x50, 0x30, 0x00 },   /* , */
    { 0x10, 0x10, 0x10, 0x10, 0x10 },   /* - */
    { 0x00, 0x60, 0x60, 0x00, 0x00 },   /* . */
    { 0x20, 0x10, 0x08, 0x04, 0x02 },   /* / */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E },   /* 0 */
    { 0x00, 0x42, 0x7F, 0x40, 0x00 },   /* 1 */
    { 0x42, 0x61, 0x51, 0x49, 0x46 },   /* 2 */
    { 0x21, 0x41, 0x45, 0x4B, 0x31 },   /* 3 */
    { 0x18, 0x14, 0x12, 0x7F, 0x10 },   /* 4 */
    { 0x27, 0x45, 0x45, 0x45, 0x39 },   /* 5 */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 },   /* 6 */
    { 0x01, 0x71, 0x09, 0x05, 0x03 },   /* 7 */
    { 0x36, 0x49, 0x49, 0x49, 0x36 },   /* 8 */
    { 0x06, 0x49, 0x49, 0x29, 0x1E },   /* 9 */
    { 0x00, 0x36, 0x36, 0x00, 0x00 },   /* : */
    { 0x00, 0x56, 0x36, 0x00, 0x00 },   /* ; */
    { 0x08, 0x14, 0x22, 0x41, 0x00 },   /* < */
    { 0x14, 0x14, 0x14, 0x14, 0x14 },   /* = */
    { 0x00, 0x41, 0x22, 0x14, 0x08 },   /* > */
    { 0x02, 0x01, 0x51, 0x09, 0x06 },   /* ? */
    { 0x32, 0x49, 0x59, 0x51, 0x3E },   /* @ */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E },   /* A */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 },   /* B */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 },   /* C */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C },   /* D */
    { 0x7F, 0x49, 0x49, 0x49, 0x41 },   /* E */
    { 0x7F, 0x09, 0x09, 0x09, 0x01 },   /* F */
    { 0x3E, 0x41, 0x49, 0x49, 0x7A },   /* G */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F },   /* H */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 },   /* I */
    { 0x20, 0x40, 0x41, 0x3F, 0x01 },   /* J */
    { 0x7F, 0x08, 0x14, 0x22, 0x41 },   /* K */
    { 0x7F, 0x40, 0x40, 0x40, 0x40 },   /* L */
    { 0x7F, 0x02, 0x0C, 0x02, 0x7F },   /* M */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F },   /* N */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E },   /* O */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 },   /* P */
    { 0x3E, 0x41, 0x51, 0x21, 0x5E },   /* Q */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 },   /* R */
    { 0x46, 0x49, 0x49, 0x49, 0x31 },   /* S */
    { 0x01, 0x01, 0x7F, 0x01, 0x01 },   /* T */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F },   /* U */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F },   /* V */
    { 0x3F, 0x40, 0x38, 0x40, 0x3F },   /* W */
    { 0x63, 0x14, 0x08, 0x14, 0x63 },   /* X */
    { 0x07, 0x08, 0x70, 0x08, 0x07 },   /* Y */
    { 0x61, 0x51, 0x49, 0x45, 0x43 },   /* Z */
    { 0x00, 0x7F, 0x41, 0x41, 0x00 },   /* [ */
    { 0x55, 0x2A, 0x55, 0x2A, 0x55 },   /* "Yen" */
    { 0x00, 0x41, 0x41, 0x7F, 0x00 },   /* ] */
    { 0x04, 0x02, 0x01, 0x02, 0x04 },   /* ^ */
    { 0x40, 0x40, 0x40, 0x40, 0x40 },   /* _ */
    { 0x00, 0x01, 0x02, 0x04, 0x00 },   /* ' */
    { 0x20, 0x54, 0x54, 0x54, 0x78 },   /* a */
    { 0x7F, 0x48, 0x44, 0x44, 0x38 },   /* b */
    { 0x38, 0x44, 0x44, 0x44, 0x20 },   /* c */
    { 0x38, 0x44, 0x44, 0x48, 0x7F },   /* d */
    { 0x38, 0x54, 0x54, 0x54, 0x18 },   /* e */
    { 0x08, 0x7E, 0x09, 0x01, 0x02 },   /* f */
    { 0x0C, 0x52, 0x52, 0x52, 0x3E },   /* g */
    { 0x7F, 0x08, 0x04, 0x04, 0x78 },   /* h */
    { 0x00, 0x44, 0x7D, 0x40, 0x00 },   /* i */
    { 0x20, 0x40, 0x44, 0x3D, 0x00 },   /* j */
    { 0x7F, 0x10, 0x28, 0x44, 0x00 },   /* k */
    { 0x00, 0x41, 0x7F, 0x40, 0x00 },   /* l */
    { 0x7C, 0x04, 0x18, 0x04, 0x78 },   /* m */
    { 0x7C, 0x08, 0x04, 0x04, 0x78 },   /* n */
    { 0x38, 0x44, 0x44, 0x44, 0x38 },   /* o */
    { 0x7C, 0x14, 0x14, 0x14, 0x08 },   /* p */
    { 0x08, 0x14, 0x14, 0x18, 0x7C },   /* q */
    { 0x7C, 0x08, 0x04, 0x04, 0x08 },   /* r */
    { 0x48, 0x54, 0x54, 0x54, 0x20 },   /* s */
    { 0x04, 0x3F, 0x44, 0x40, 0x20 },   /* t */
    { 0x3C, 0x40, 0x40, 0x20, 0x7C },   /* u */
    { 0x1C, 0x20, 0x40, 0x20, 0x1C },   /* v */
    { 0x3C, 0x40, 0x30, 0x40, 0x3C },   /* w */
    { 0x44, 0x28, 0x10, 0x28, 0x44 },   /* x */
    { 0x0C, 0x50, 0x50, 0x50, 0x3C },   /* y */
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }    /* z */
};

#endif  /*  _PCD8544_H_ */
