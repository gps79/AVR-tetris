/*
 * tetris.c
 *
 * Created: 30.11.2016 19:12:13
 * Author : Grzegorz Pietrusiak
 */ 
#define SHOW_GAME_OVER FALSE
#define TETRIMINOS_AS_BOXES FALSE

#define __ASSERT_USE_STDERR
//#define NDEBUG

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include "my_assert.h"

#include "pcd8544.h"
#include "pcd8544.c"

uint8_t tetriminos[7*4] = { // there are 7 tetriminos, each of them has 4 orientations (0, 90deg., 180deg., and 270deg.)
	0xE0, 0x92, 0xE0, 0x92, // I is only 3 blocks long due to optimization
	0xE4, 0xD2, 0x9C, 0x4B, // J
	0xF0, 0x93, 0x3C, 0xC9, // L
	0xD8, 0xD8, 0xD8, 0xD8, // o
	0x78, 0x99, 0x78, 0x99, // S
	0xE8, 0x9A, 0x5C, 0x59, // T
	0xCC, 0x5A, 0xCC, 0x5A  // Z
};

uint8_t currentTetrimino; // tetrimino currently being dropped
uint8_t currentTetriminoPosition; // linear position of tetrimino on screen calculated as x+8*y
uint8_t nextTetrimino; // tetrimino next to be dropped once current finished
#define NEXT_TETRIMINO_POSITION 155

uint8_t matrix[16] =  // 16rows, 8 blocks per row, each block is represented by one bit
{
	0,0,0,0,0,0,0,0,0,0,0,0
};

#define LEFT_BUTTON_PRESSED (!(PIND & (1<<PD0)))
#define RIGHT_BUTTON_PRESSED (!(PIND & (1<<PD2)))
#define DOWN_BUTTON_PRESSED (!(PIND & (1<<PD1)))
#define ROTATION_BUTTON_PRESSED (!(PIND & (1<<PD3)))
#define TIMER_HAS_EXPIRED ((TIFR & (1 << TOV1) ) > 0)

// poor man's random function. Not too bad as we need to random tiles from the range of 0-6 only
static uint8_t myrand() // the cost is 30B (this function + ADC initialization)
{
	static uint8_t randomNumber = 3;

	// we use ADC conversion of unconnected ATMEGA ADC pin to read the noise. The noise is added (XOR) to randomized value to make it more random.
	ADCSRA |= (1<<ADSC); // run ADC conversion once
	while(ADCSRA & (1<<ADSC)); // wait until ADC conversion finishes

	// use value read from ADC as additional seed for the randomizer
//	randomNumber = ((randomNumber*109+89)%255)^ADCL; // better randomizer but 12 Bytes more expensive than the vbelow one
//	randomNumber = (randomNumber*109)^ADC; // a bit worse randomizer but we save 12 Bytes
	randomNumber = ((randomNumber<<4)|(randomNumber>>4))^ADC; // even worse randomizer but we save another bytes
//	randomNumber = randomNumber^ADC; // this one is too bad to use it
	return randomNumber;
}

void startTimer()
{
	TIFR |= (1 << TOV1); // reset the overflow flag (by writing '1')
	TCNT1 = 65535-975*4/* MHz */;
	TCCR1B = (1 << CS10) | (1 << CS12); // start the timer by setting 1024 prescaler
}

static void randomizeNextTetrimino()
{
	currentTetrimino = nextTetrimino;
	currentTetriminoPosition = 3; // top middle initial position of current tetrimino
	nextTetrimino = (myrand()%7) << 2;
}

static void gameInit()
{
	// init ADC0 which supports random number generator
	ADMUX = (1 << REFS0) | (1 << REFS1); // ADC0 + internal 2.56V reference
	ADCSRA = (1 << ADPS2) | (1 << ADPS1) // 64 prescaler for 4Mhz
			| (1 << ADEN);    // Enable the ADC

	// init hardware buttons
	//	DDRD = 0x00; // not needed?? (works without this line) Atmega has this by default?

	LcdInit();
	randomizeNextTetrimino(); // run twice to random both current and next tetriminos
	randomizeNextTetrimino();

	// initialize the timer
	startTimer(); // inform after 1 second period
}

static void drawTile (uint8_t x, uint8_t y)
{
	assert(x<8);
	assert(y<21);

	uint8_t scrX = y*4;
	uint8_t scrY = 48-(8 + x*4)-4;

	LcdBar(scrX, scrY, 4,4);
	if (TETRIMINOS_AS_BOXES) // -32B
	{
		LcdSetPen(PIXEL_OFF);
		LcdBar(scrX+1, scrY+1, 2,2);
		LcdSetPen(PIXEL_ON);
	}
}

// tetriminoId contains 3 bits of tetrimino number and 2 bits of orientation
//   bits in tetrominoId:   MSB  000NNNOO LSB
//                              NNN - 3 bits of tetrimino number (values 0-7)
//                               OO - 2 bits of tetrimino orientation (values 0-3)
// position is a linear position on screen (16 rows by 8 columns)
//   bits in position:     MSB  0RRRRCCC LSB
//                              RRRR - 4 bits of row number (values 0-15)
//                               CCC - 3 bits of column number (values 0-7)
static void drawTetrimino(uint8_t tetriminoId, uint8_t position)
{
	uint8_t x,y,tetriminoSpec, bitMask;
	assert(tetriminoId<7*4);
	assert(position<16*8 || position==NEXT_TETRIMINO_POSITION);

	// example tetrimino specification coded as 0x9A ( 01011100 binary; or 010 111 000 as three rows)
	//  .#.
	//  ###
	//  ...
	tetriminoSpec = tetriminos[tetriminoId];
	
	// draw
	x = position & 0x07;
	y = position >> 3;
	for (bitMask = 0x80; bitMask != 0; bitMask >>= 1)
	{
		if (tetriminoSpec&bitMask)
		{
			drawTile(x,y);
		}
		++x;
		if (bitMask&0x24)
		{
			x-=3;
			++y;
		}
	}
}

// this function plays two roles depending on the value of "storePermanently"
// When storePermanently==false:
//   it behaves like a function which tests if "tetrimino" can be placed on screen in "position" (i.e. does not collide with other objects on screen)
// When storePermanently==true:
//   it behaves like a procedure which stores given "tetrimino" in "position" in the "matrix".
//   To run in this mode you have to be sure that you can store the "tetrimino" in the "position" by running this function in storePermanently==false mode first
static bool canPlaceTetrimino(uint8_t tetrimino, uint8_t position, bool storePermanently)
{
	assert(position<136);
	assert(tetrimino<7*4);

	uint8_t bitMask;
	uint8_t tetriminoSpec = tetriminos[tetrimino];
	uint8_t xPos = position & 0x7;
	uint8_t yPos = position >> 3;
	if (yPos>=16)
	{
		return FALSE;
	}
	for (bitMask = 0x80; bitMask != 0; bitMask >>= 1)
	{
		assert(xPos<10);
		assert(yPos<18);
		assert(y<3);
		if (tetriminoSpec&bitMask)
		{
			if (storePermanently)
			{
				matrix[yPos] |= (0x80>>xPos);
			}
			else
			{
				if (xPos >= 8)
				{
					return FALSE;
				}
				if (yPos >= 16)
				{
					return FALSE;
				}
				if (matrix[yPos] & (0x80>>xPos))
				{
					return FALSE;
				}
			}
		}

		++xPos;
		if (bitMask & 0x24) // go to next line and correct "xPos" and "yPos"
		{
			xPos -= 3;
			++yPos;
		}
	}
	return TRUE;
}

static void moveTetriminoDown()
{
	uint8_t newPosition = currentTetriminoPosition+8; // next row
	if (canPlaceTetrimino(currentTetrimino, newPosition, FALSE))
	{
		currentTetriminoPosition = newPosition;
	}
	else
	{ // store current tetrimino permanently (in the "matrix") in current location
		canPlaceTetrimino(currentTetrimino, currentTetriminoPosition, TRUE);

		// verify if there is any full line to drop
		uint8_t row;
		for (row=15; row!=255; --row)
		{
			while (matrix[row] == 0xff) // we found a row full of tiles; "while" loop is used to remove all full lines dropped to "row" position
			{
				// drop all rows above "row" one row down
				uint8_t rowUp;
				for (rowUp=row; rowUp>0; --rowUp)
				{
					matrix[rowUp] = matrix[rowUp-1];
				}
				matrix[0] = 0;
			}
		}
		randomizeNextTetrimino();
		if (!canPlaceTetrimino(currentTetrimino, currentTetriminoPosition, FALSE))
		{
			// GAME OVER
			if (SHOW_GAME_OVER)
			{
				for (uint16_t i = 0; i<48*84/8; i+=2)
				{
					LcdCache[i] &= 0xAA;
					LcdCache[i+1] &= 0x55;
				}
				LcdUpdate();
			}
			abort();
		}
	}
}

static void displayScene()
{
	// display screen decoration
	memset(LcdCache,0x00,LCD_CACHE_SIZE); // LcdClear();

	LcdSetPen(PIXEL_ON);
	LcdBar(0,0,72,48);
	LcdSetPen(PIXEL_OFF);
	LcdBar(0,7,65,34);

	// draw all tiles dropped till now
	LcdSetPen(PIXEL_ON);
	uint8_t * lineAddr = &matrix[15];
	uint8_t y=16;
	while(y)
	{
		--y;
		uint8_t bitMask = 0x01;
		uint8_t x=8;
		while (x)
		{
			--x;
			if ((*lineAddr)&bitMask)
			{
				drawTile(x,y);
			}
			bitMask <<= 1;
		}
		--lineAddr;
	}

	// draw current tile
	drawTetrimino(currentTetrimino, currentTetriminoPosition);

	// draw next tetrimino
	drawTetrimino(nextTetrimino, NEXT_TETRIMINO_POSITION);

	LcdUpdate(); // draw from buffer to the LCD
}

static void mydelay()
{
	uint32_t t = 205535;//12000;
	while (--t)
	{
		__asm__ __volatile__("");
	}
}

//#define MCU_TROUBLESHOOTING
#ifdef MCU_TROUBLESHOOTING
#define LEDON (PORTC |=(1<<5))
#define LEDOFF (PORTC &= ~(1 << 5))
#endif // MCU_TROUBLESHOOTING
int main() 
{
	#ifdef MCU_TROUBLESHOOTING
	DDRC = 0xFF;while(1){LEDON;_delay_ms(500);LEDOFF;_delay_ms(500);}
	#endif // MCU_TROUBLESHOOTING
	gameInit();

	bool isDelay = FALSE;
	while(1)
	{
		displayScene();
		if (isDelay)
		{
			mydelay();
			isDelay = FALSE;
		}

		if ((TIMER_HAS_EXPIRED) || (DOWN_BUTTON_PRESSED))
		{
			moveTetriminoDown();
			startTimer(); // inform after 1 second period
			isDelay = TRUE;
		}

		if (ROTATION_BUTTON_PRESSED)
		{
			uint8_t newTetrimino;
			if ((currentTetrimino&0x03) == 0x03)
			{
				newTetrimino = currentTetrimino & 0xfC;
			} 
			else
			{
				newTetrimino = currentTetrimino + 1;
			}
			if (canPlaceTetrimino(newTetrimino, currentTetriminoPosition, FALSE))
			{
				currentTetrimino = newTetrimino;
				isDelay = TRUE;
			}
		}
		uint8_t newPosition;
		if (LEFT_BUTTON_PRESSED)
		{
			if ((currentTetriminoPosition&0x07) != 0)
			{
				newPosition = currentTetriminoPosition - 1;
				goto labelNewPosition;
			}
		}

		if (RIGHT_BUTTON_PRESSED)
		{
			if ((currentTetriminoPosition&0x07) != 0x07)
			{
				newPosition = currentTetriminoPosition + 1;
labelNewPosition:
				if (canPlaceTetrimino(currentTetrimino, newPosition, FALSE))
				{
					currentTetriminoPosition = newPosition;
					isDelay = TRUE;
				}
			}
		}

	}	
	return 0;
}

