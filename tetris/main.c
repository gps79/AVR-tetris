/*
 * tetris.c
 *
 * Created: 30.11.2016 19:12:13
 * Author : Grzegorz Pietrusiak
 */ 
#define SHOW_GAME_OVER FALSE

# define F_CPU 4000000UL

#define __ASSERT_USE_STDERR
//#define NDEBUG

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdlib.h>
#include "my_assert.h"

#include "pcd8544.h"

uint8_t tetriminos[7][4] = { // there are 7 tetriminos, each of them has 4 orientations (0, 90deg., 180deg., and 270deg.)
	{ 0xE0, 0x92, 0xE0, 0x92}, // I is only 3 blocks long due to optimization
	{ 0xE4, 0xD2, 0x9C, 0x4B}, // J
	{ 0xF0, 0x93, 0x3C, 0xC9}, // L
	{ 0xD8, 0xD8, 0xD8, 0xD8}, // o
	{ 0x78, 0x99, 0x78, 0x99}, // S
	{ 0xE8, 0x9A, 0x5C, 0x59}, // T
	{ 0xCC, 0x5A, 0xCC, 0x5A}  // Z
};

uint8_t currentTetrimino; // tetrimino currently being dropped
uint8_t currentTetriminoPosition; // linear position of tetrimino on screen calculated as x+8*y
uint8_t nextTetrimino; // tetrimino next to be dropped once current finished
#define NEXT_TETRIMINO_POSITION 155

uint8_t matrix[16] =  // 16rows, 8 blocks per row, each block is represented by one bit
{
	0,0,0,0,0,0,0,0,0,0,0,0
};
uint16_t score = 0;

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

void startTimer(uint16_t delay)
{
	TIFR |= (1 << TOV1); // reset the overflow flag (by writing '1')
	TCNT1 = delay;
	TCCR1B = (1 << CS10) | (1 << CS12); // start the timer by setting 1024 prescaler
}

void gameInit()
{
	// init ADC0 which supports random number generator
	ADMUX = (1 << REFS0) | (1 << REFS1); // ADC0 + internal 2.56V reference
	ADCSRA = (1 << ADPS2) | (1 << ADPS1) // 64 prescaler for 4Mhz
			| (1 << ADEN);    // Enable the ADC

	// init hardware buttons
	//	DDRD = 0x00; // not needed?? (works without this line) Atmega has this by default?

	LcdInit();
	currentTetrimino = (myrand()%7) << 2;
	currentTetriminoPosition = 3; // top middle initial position of current tetrimino
	nextTetrimino = (myrand()%7) << 2;

	// initialize the timer
	startTimer(65535-3902); // inform after 1 second period
}

void drawTile (uint8_t x, uint8_t y)
{
	assert(x<8);
	assert(y<21);

	uint8_t scrX = y*4;
	uint8_t scrY = 48-(8 + x*4)-4;

	LcdBar(scrX, scrY, 4,4);
	LcdSetPen(PIXEL_OFF);
	LcdBar(scrX+1, scrY+1, 2,2);
	LcdSetPen(PIXEL_ON);
}

void drawTileLinearly (uint8_t pos)
{
	assert(pos<8*16 || (pos>=NEXT_TETRIMINO_POSITION && pos<=(NEXT_TETRIMINO_POSITION+10)));
	uint8_t x = pos & 0x07;
	uint8_t y = pos >> 3;
	drawTile(x, y);
}

// tetriminoId contains 3 bits of tetrimino number and 2 bits of orientation
//   bits in tetrominoId:   MSB  000NNNOO LSB
//                              NNN - 3 bits of tetrimino number (values 0-7)
//                               OO - 2 bits of tetrimino orientation (values 0-3)
// position is a linear position on screen (16 rows by 8 columns)
//   bits in position:     MSB  0RRRRCCC LSB
//                              RRRR - 4 bits of row number (values 0-15)
//                               CCC - 3 bits of column number (values 0-7)
void drawTetrimino(uint8_t tetriminoId, uint8_t position)
{
	uint8_t x,y,bitMask;
	assert(tetriminoId<7*4);
	assert(position<16*8 || position==NEXT_TETRIMINO_POSITION);

	// example tetrimino specification coded as 0x9A ( 01011100 binary; or 010 111 000 as three rows)
	//  .#.
	//  ###
	//  ...
	uint8_t tetriminoSpec = tetriminos[tetriminoId>>2][tetriminoId&0x03];
	
	// draw
	x=0;y=0;
	for (bitMask = 0x80; bitMask != 0; bitMask >>= 1)
	{
		if (tetriminoSpec&bitMask)
		{
			drawTileLinearly(position + y*8 + x);
		}

		++x;
		if (x==3)
		{
			x = 0;
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
bool canPlaceTetrimino(uint8_t tetrimino, uint8_t position, bool storePermanently)
{
	assert(position<136);
	assert(tetrimino<7*4);

	uint8_t x,y,bitMask;
	uint8_t tetriminoSpec = tetriminos[tetrimino>>2][tetrimino&0x03];
	uint8_t xPos = position&0x07;
	uint8_t yPos = position>>3;
	if (yPos>=16)
	{
		return FALSE;
	}
	x=0;y=0;
	for (bitMask = 0x80; bitMask != 0; bitMask >>= 1)
	{
		assert(xPos<10);
		assert(yPos<18);
		assert(x<3);
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

		++x;
		++xPos;
		if (x==3) // go to next line and correct "x" and "xPos"
		{
			x = 0;
			++y;
			xPos -= 3;
			++yPos;
		}
	}
	return TRUE;
}

void moveTetriminoDown()
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

				// add some scores here due to full row
				++score;
			}
		}
		currentTetrimino = nextTetrimino;
		currentTetriminoPosition = 3; // top middle initial position of current tetrimino
		nextTetrimino = (myrand()%7) << 2;
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

void displayScene()
{
	uint8_t x,y;
	// display screen decoration
	LcdClear();
	LcdSetPen(PIXEL_ON);
	LcdBar(0,0,72,48);
	LcdSetPen(PIXEL_OFF);
	LcdBar(0,7,65,34);

	// draw all tiles dropped till now
	LcdSetPen(PIXEL_ON);
	for (y=0;y<16;++y)
	{
		for (x=0;x<8;++x)
		{
			if (matrix[y]&(0x80>>x))
			{
				drawTile(x,y);
			}
		}
	}

	// draw current tile
	drawTetrimino(currentTetrimino, currentTetriminoPosition);

	// draw next tetrimino
	drawTetrimino(nextTetrimino, NEXT_TETRIMINO_POSITION);

	// display score
	// ... TODO

	LcdUpdate(); // draw from buffer to the LCD
}

int main()
{
	gameInit();

	bool isDelay = FALSE;
	while(1)
	{
		displayScene();
		if (isDelay)
		{
			_delay_ms(80);
			isDelay = FALSE;
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
		if (LEFT_BUTTON_PRESSED)
		{
			uint8_t newPosition;
			if ((currentTetriminoPosition&0x07) != 0)
			{
				newPosition = currentTetriminoPosition - 1;
				if (canPlaceTetrimino(currentTetrimino, newPosition, FALSE))
				{
					currentTetriminoPosition = newPosition;
					isDelay = TRUE;
				}
			}
		}

		if (RIGHT_BUTTON_PRESSED)
		{
			// how far we can go to the right depends on tetrimino shape and rotation. 
			// For most of them we can go up to position 5 (for 0 and 180 degrees rotation) and up to 6 (for 90 and 270 degrees rotation).
			// Two tetriminos are different: "O" shape and "I" shape. For "O" shape we go up to 6 position (regardless of the rotation).
			// For "I" shape we go up to 7 position (for 90 and 270 degrees) or up to 5 position (for 0 and 180 degrees)
			// Below there is a truth table for the decision "if we can move right" (1) or not (0):
			//                           current X position
			// orientation   shape no.    0 1 2 3 4 5 6 7
			// -------------------------------------------
			//     0            0         1 1 1 1 1 0 0 0
			//     0            1         1 1 1 1 1 0 0 0
			//     0            2         1 1 1 1 1 0 0 0
			//     0            3         1 1 1 1 1 1 0 0
			//     0            4         1 1 1 1 1 0 0 0
			//     0            5         1 1 1 1 1 0 0 0
			//     0            6         1 1 1 1 1 0 0 0
			//
			//     1            0         1 1 1 1 1 1 1 0
			//     1            1         1 1 1 1 1 1 0 0
			//     1            2         1 1 1 1 1 1 0 0
			//     1            3         1 1 1 1 1 1 0 0
			//     1            4         1 1 1 1 1 1 0 0
			//     1            5         1 1 1 1 1 1 0 0
			//     1            6         1 1 1 1 1 1 0 0
			uint8_t shapeNo = currentTetrimino >> 2;
			uint8_t orientation = currentTetrimino & 1; // one bit is enough as tetrimino boundaries are the same for (0 and 180) degrees, and for (90 and 270) degrees
			uint8_t xPos = currentTetriminoPosition&0x07; // current tetrimino X position (0-7)
			if (   ((orientation==1) && ((xPos<6) || ((xPos==6) && (shapeNo==0)))) ||
				   ((orientation!=1) && ((xPos<5) || ((xPos==5) && (shapeNo==3))))   ) // this condition is a result of above truth table optimization
			{
				uint8_t newPosition = currentTetriminoPosition + 1;
				if (canPlaceTetrimino(currentTetrimino, newPosition, FALSE))
				{
					currentTetriminoPosition = newPosition;
					isDelay = TRUE;
				}
			}
		}

		if (DOWN_BUTTON_PRESSED)
		{
			moveTetriminoDown();
			startTimer(65535-3902); // inform after 1 second period
			isDelay = TRUE;
		}

		if (TIMER_HAS_EXPIRED)
		{
			moveTetriminoDown();
			startTimer(65535-3902); // inform after 1 second period
		}
	}	
	return 0;
}

