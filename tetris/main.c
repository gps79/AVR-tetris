/*
 * tetris.c
 *
 * Created: 30.11.2016
 * Author : Grzegorz Pietrusiak <gpsspam2@gmail.com>
 * First release version ready: 11.12.2016
 * License      :  GPL
 */

#define __ASSERT_USE_STDERR
//#define NDEBUG

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include "my_assert.h"

#include "pcd8544.h"
#include "pcd8544.c"

typedef enum
{
	check = 0,
	store = 1,
	draw = 2
} TStoreMode;

const uint8_t tetriminos[8*4] = { // there are 8 tetriminos, each of them has 4 orientations (0, 90deg., 180deg., and 270deg.)
	0xE0, 0x92, 0xE0, 0x92, // I is only 3 blocks long due to optimization
	0xE4, 0xD2, 0x9C, 0x4B, // J
	0xF0, 0x93, 0x3C, 0xC9, // L
	0xD8, 0xD8, 0xD8, 0xD8, // o
	0x78, 0x99, 0x78, 0x99, // S
	0xE8, 0x9A, 0x5C, 0x59, // T
	0xCC, 0x5A, 0xCC, 0x5A, // Z
	0x80, 0x80, 0x80, 0x80  // . additional shape which is not a real tetrimino, but it adds some fun to the game
};

// Global variables
uint8_t currentTetrimino; // tetrimino currently being dropped
uint8_t currentTetriminoPosition; // linear position of tetrimino on screen calculated as x+8*y
uint8_t nextTetrimino; // tetrimino next to be dropped once current finished
#define NEXT_TETRIMINO_POSITION (3 + 8*19) // (X + 8*Y) position

uint8_t matrix[16] =  // 16rows, 8 blocks per row, each block is represented by one bit
{
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

uint8_t g_score = 0;

#define LEFT_BUTTON_PRESSED (PIND & (1<<PD0)) // returns TRUE if left button is pressed
#define RIGHT_BUTTON_PRESSED (PIND & (1<<PD2)) // returns TRUE if right button is pressed
#define DOWN_BUTTON_PRESSED (PIND & (1<<PD1)) // returns TRUE if down button is pressed
#define ROTATION_BUTTON_PRESSED (PIND & (1<<PD3)) // returns TRUE if rotation button is pressed
#define TIMER_HAS_EXPIRED ((TIFR & (1 << TOV1) ) > 0) // returns TRUE if timer has expired

static uint8_t myrand()
{
	static uint8_t g_randomNumber; // uninitialized value; it is ok to be random at init :)
	// we use ADC conversion of unconnected ATMEGA ADC pin to read the noise. The noise is added (XOR) to randomized value to make it more random.
	ADCSRA |= (1<<ADSC); // run ADC conversion once
	while(ADCSRA & (1<<ADSC)); // wait until ADC conversion finishes

	g_randomNumber = (g_randomNumber<<1)^ADC;
	return g_randomNumber & 0x1C;
}

void startTimer()
{
	TIFR = (1 << TOV1); // reset the overflow flag (by writing '1')
	TCNT1 = 65535-3580+(g_score*10); // starting from about 0.5s period
	TCCR1B = (1 << CS10) | (1 << CS12); // start the timer by setting 1024 prescaler
}

static void randomizeNextTetrimino()
{
	currentTetrimino = nextTetrimino;
	currentTetriminoPosition = 3; // top middle initial position of current tetrimino
	nextTetrimino = myrand();

	assert((currentTetrimino >> 2) < 8);	// check if correctly randomized
	assert((currentTetrimino & 0x03) == 0); // ...
	assert((nextTetrimino >> 2) < 8);		// ...
	assert((nextTetrimino & 0x03) == 0);	// ...
}

static void gameInit()
{
	// initialize ADC0 which supports random number generator
	ADMUX = (1 << REFS0) | (1 << REFS1); // ADC0 + internal 2.56V reference
	ADCSRA = (1 << ADPS2) | (1 << ADPS1) // 64 prescaler
			| (1 << ADEN);    // Enable the ADC

	for (uint8_t i = 8; i; --i)
	{
		randomizeNextTetrimino(); // run it a couple of times to get really random values
	}

	LcdInit();

	// initialize the timer
	startTimer(); // start the timer means to start the game play
}

static void drawTile (uint8_t x, uint8_t y)
{
	assert(x<8);
	assert(y<21);

	uint8_t scrX = y*4; // convert virtual coordinates to screen coordinates
	uint8_t scrY = 48-(8 + x*4)-4;

	LcdBar(scrX, scrY, 4,4);
	LcdBar(scrX+1, scrY+1, 2,2);
}

// this function works in three modes depending on the value of "storePermanently"
// When storePermanently==check:
//   it behaves like a function which tests if "tetrimino" can be placed on screen in "position" (i.e. does not collide with other objects on screen)
// When storePermanently==store:
//   it behaves like a procedure which stores given "tetrimino" in "position" in the "matrix".
//   To run in this mode you have to be sure that you can store the "tetrimino" in the "position" by running this function in "check" mode first
// When storePermanently==draw:
//   it draws tetrimino in "position" without checking anything or touching the "matrix"
//
// "tetrimino" contains 3 bits of tetrimino number and 2 bits of orientation
//   bits in tetrimino:     MSB  000NNNOO LSB
//                              NNN - 3 bits of tetrimino number (values 0-7)
//                               OO - 2 bits of tetrimino orientation (values 0-3)
// "position" is a linear position on screen (16 rows by 8 columns)
//   bits in position:     MSB  0RRRRCCC LSB
//                              RRRR - 4 bits of row number (values 0-15)
//                               CCC - 3 bits of column number (values 0-7)
//   "position" can be set to one special value NEXT_TETRIMINO_POSITION for drawing next tetrimino on the bottom of the screen
//
static bool canPlaceTetrimino(uint8_t tetrimino, uint8_t position, TStoreMode storePermanently)
{
	assert(position<136 || position==NEXT_TETRIMINO_POSITION);
	assert(tetrimino<8*4);

	// example tetrimino specification coded as 0x9A ( 01011100 binary; or 010 111 000 as three rows)
	//  .#.
	//  ###
	//  ...
	uint8_t tetriminoSpec = tetriminos[tetrimino];

	uint8_t xPos = position & 0x7; // split position into X and Y coordinates
	uint8_t yPos = position >> 3;
	if ((!storePermanently) && (yPos>=16)) // check yPos only in "check" mode
	{
		return FALSE;
	}
	uint8_t bitMask;
	for (bitMask = 0x80; bitMask != 0; bitMask >>= 1)
	{
		assert(xPos<10);
		uint8_t matrixMask = 0x80 >> xPos;
		//assert(yPos<18);
		if (tetriminoSpec&bitMask)
		{
			if (storePermanently == draw)
			{
				drawTile(xPos, yPos);
			}
			else if (storePermanently == store)
			{
				matrix[yPos] |= matrixMask;
			}
			else // if (storePermanently == check)
			{
				if (xPos >= 8)
				{
					return FALSE;
				}
				if (yPos >= 16)
				{
					return FALSE;
				}
				if (matrix[yPos] & matrixMask)
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
	if (canPlaceTetrimino(currentTetrimino, newPosition, check))
	{
		currentTetriminoPosition = newPosition;
	}
	else
	{ // store current tetrimino permanently (in the "matrix") in current location
		canPlaceTetrimino(currentTetrimino, currentTetriminoPosition, store);

		// verify if there is any full line to drop
		uint8_t row;
		for (row=15; row!=255; --row)
		{
			while (matrix[row] == 0xff) // we found a row full of tiles; "while" loop is used to remove all full lines dropped to "row" position
			{
				++g_score;
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
		if (!canPlaceTetrimino(currentTetrimino, currentTetriminoPosition, check))
		{
			// GAME OVER
			while(1){}; // go to infinite loop
		}
	}
}


static void showScore()
{
	LcdBar(2,3,64-(g_score>>2), 1);
}

static void displayScene()
{
	// display screen decoration
	memset(LcdCache,0x00,LCD_CACHE_SIZE);  // clear LCD screen buffer

	LcdBar(0,0,72,48);
	LcdBar(0,7,65,34);

	// draw all tiles dropped till now
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
	canPlaceTetrimino(currentTetrimino, currentTetriminoPosition, draw);

	// draw next tetrimino
	canPlaceTetrimino(nextTetrimino, NEXT_TETRIMINO_POSITION, draw);

	showScore();

	LcdUpdate(); // move the content from screen buffer to the LCD driver memory in order to display
}

static void delayIfButtonPressed()
{
	uint32_t t = 165535; // tuned to get the right timing in button repetition
	while (--t && (PIND))
	{
	}
}

int main() 
{
	gameInit();

	while (1)
	{
		if ((TIMER_HAS_EXPIRED) || (DOWN_BUTTON_PRESSED))
		{
			moveTetriminoDown();
			startTimer();
		}
		if (ROTATION_BUTTON_PRESSED)
		{
			uint8_t newTetrimino;
			if ((currentTetrimino&0x03) == 0x00)
			{
				newTetrimino = currentTetrimino | 0x03;
			} 
			else
			{
				newTetrimino = currentTetrimino - 1;
			}
			if (canPlaceTetrimino(newTetrimino, currentTetriminoPosition, check))
			{
				currentTetrimino = newTetrimino;
			}
		}
		uint8_t newPosition;
		if (LEFT_BUTTON_PRESSED)
		{
			if ((currentTetriminoPosition&0x07) != 0)
			{
				newPosition = currentTetriminoPosition - 1;
				goto labelNewPosition; // this is not a good practice, but it was needed for optimization
			}
		}

		if (RIGHT_BUTTON_PRESSED)
		{
			newPosition = currentTetriminoPosition + 1;
			if ((newPosition & 0x07) != 0) // if it is 0 it means that currentTetriminoPosition is on the very right end and we can't move to the right anymore
			{
labelNewPosition:
				if (canPlaceTetrimino(currentTetrimino, newPosition, check))
				{
					currentTetriminoPosition = newPosition;
				}
			}
		}

		if (PIND) // any button is pressed
		{
			displayScene();
			delayIfButtonPressed();
		}
		else
		{
			displayScene();
		}
	}	
	return 0;
}

