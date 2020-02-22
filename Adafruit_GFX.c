/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */

#include "Adafruit_GFX.h"
#include "glcdfont.c"
#ifdef __AVR__
#include <avr/pgmspace.h>
#elif defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#endif

// Many (but maybe not all) non-AVR board installs define macros
// for compatibility with existing PROGMEM-reading AVR code.
// Do our own checks and defines here for good measure...

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif

// Pointers are a peculiar case...typically 16-bit on AVR boards,
// 32 bits elsewhere.  Try to accommodate both...

#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

inline GFXglyph *pgm_read_glyph_ptr( const GFXfont *gfxFont, uint8_t c )
{
#ifdef __AVR__
	return &( ( ( GFXglyph * )pgm_read_pointer( &gfxFont->glyph ) )[c] );
#else
	// expression in __AVR__ section may generate "dereferencing type-punned
	// pointer will break strict-aliasing rules" warning In fact, on other
	// platforms (such as STM32) there is no need to do this pointer magic as
	// program memory may be read in a usual way So expression may be simplified
	return gfxFont->glyph + c;
#endif //__AVR__
}

inline uint8_t *pgm_read_bitmap_ptr( const GFXfont *gfxFont )
{
#ifdef __AVR__
	return ( uint8_t * )pgm_read_pointer( &gfxFont->bitmap );
#else
	// expression in __AVR__ section generates "dereferencing type-punned pointer
	// will break strict-aliasing rules" warning In fact, on other platforms (such
	// as STM32) there is no need to do this pointer magic as program memory may
	// be read in a usual way So expression may be simplified
	return gfxFont->bitmap;
#endif //__AVR__
}

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

/* STATIC VARS */
static int16_t WIDTH,      ///< This is the 'raw' display width - never changes
       HEIGHT;         ///< This is the 'raw' display height - never changes
static int16_t _width,     ///< Display width as modified by current rotation
       _height,        ///< Display height as modified by current rotation
       cursor_x,       ///< x location to start print()ing text
       cursor_y;       ///< y location to start print()ing text
static uint16_t textcolor, ///< 16-bit background color for print()
       textbgcolor;    ///< 16-bit text color for print()
static uint8_t textsize_x, ///< Desired magnification in X-axis of text to print()
       textsize_y,     ///< Desired magnification in Y-axis of text to print()
       rotation;       ///< Display rotation (0 thru 3)
static bool wrap,       ///< If set, 'wrap' text at right edge of display
       _cp437;         ///< If set, use correct CP437 charset (default is off)
static GFXfont *gfxFont;   ///< Pointer to special font

/**************************************************************************/
/*!
   @brief    Instatiate a GFX context for graphics
   @param    driver   Pointer to low level driver implementation
   @param    w   Display width, in pixels
   @param    h   Display height, in pixels
*/
/**************************************************************************/
void Adafruit_GFX_Init( Adafruit_GFX_Driver_t *driver, int16_t w, int16_t h )
{
	WIDTH = w;
	HEIGHT = h;
	_width = WIDTH;
	_height = HEIGHT;
	rotation = 0;
	cursor_y = 0;
	cursor_x = cursor_y;
	textsize_x = 1;
	textsize_y = textsize_x;
	textcolor = 0xFFFF;
	textbgcolor = textcolor;
	wrap = true;
	_cp437 = false;
	gfxFont = NULL;
}

/**************************************************************************/
/*!
   @brief    Write a line.  Bresenham's algorithm - thx wikpedia
    @param    x0  Start point x coordinate
    @param    y0  Start point y coordinate
    @param    x1  End point x coordinate
    @param    y1  End point y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_WriteLine( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             uint16_t color )
{
	if ( driver->writeLine != NULL )
	{
		driver->writeLine( x0, y0, x1, y1, color );
		return;
	}

	int16_t steep = abs( y1 - y0 ) > abs( x1 - x0 );

	if ( steep )
	{
		_swap_int16_t( x0, y0 );
		_swap_int16_t( x1, y1 );
	}

	if ( x0 > x1 )
	{
		_swap_int16_t( x0, x1 );
		_swap_int16_t( y0, y1 );
	}

	int16_t dx, dy;
	dx = x1 - x0;
	dy = abs( y1 - y0 );

	int16_t err = dx / 2;
	int16_t ystep;

	if ( y0 < y1 )
	{
		ystep = 1;
	}
	else
	{
		ystep = -1;
	}

	for ( ; x0 <= x1; x0++ )
	{
		if ( steep )
		{
			Adafruit_GFX_WritePixel( driver, y0, x0, color );
		}
		else
		{
			Adafruit_GFX_WritePixel( driver, x0, y0, color );
		}

		err -= dy;

		if ( err < 0 )
		{
			y0 += ystep;
			err += dx;
		}
	}
}

/**************************************************************************/
/*!
   @brief    Start a display-writing routine, overwrite in subclasses.
*/
/**************************************************************************/
void Adafruit_GFX_StartWrite( Adafruit_GFX_Driver_t *driver )
{
	if ( driver->startWrite != NULL )
	{
		driver->startWrite();
	}
}

/**************************************************************************/
/*!
   @brief    Write a pixel, overwrite in subclasses if startWrite is defined!
    @param   x   x coordinate
    @param   y   y coordinate
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_WritePixel( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, uint16_t color )
{
	if ( driver->writePixel != NULL )
	{
		driver->writePixel( x, y, color );
		return;
	}

	if ( driver->drawPixel != NULL )
	{
		driver->drawPixel( x, y, color );
	}
}

/**************************************************************************/
/*!
   @brief    Write a perfectly vertical line, overwrite in subclasses if
   startWrite is defined!
    @param    x   Top-most x coordinate
    @param    y   Top-most y coordinate
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_WriteFastVLine( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t h,
                                  uint16_t color )
{
	if ( driver->writeFastVLine != NULL )
	{
		driver->writeFastVLine( x, y, h, color );
		return;
	}

	// Overwrite in subclasses if startWrite is defined!
	// Can be just writeLine(x, y, x, y+h-1, color);
	// or writeFillRect(x, y, 1, h, color);
	Adafruit_GFX_DrawFastVLine( driver, x, y, h, color );
}

/**************************************************************************/
/*!
   @brief    Write a perfectly horizontal line, overwrite in subclasses if
   startWrite is defined!
    @param    x   Left-most x coordinate
    @param    y   Left-most y coordinate
    @param    w   Width in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_WriteFastHLine( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w,
                                  uint16_t color )
{
	if ( driver->writeFastHLine != NULL )
	{
		driver->writeFastHLine( x, y, w, color );
		return;
	}

	Adafruit_GFX_DrawFastHLine( driver, x, y, w, color );
}

/**************************************************************************/
/*!
   @brief    Write a rectangle completely with one color, overwrite in
   subclasses if startWrite is defined!
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_WriteFillRect( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w, int16_t h,
                                 uint16_t color )
{
	if ( driver->writeFillRect != NULL )
	{
		driver->writeFillRect( x, y, w, h, color );
		return;
	}

	// Overwrite in subclasses if desired!
	Adafruit_GFX_FillRect( driver, x, y, w, h, color );
}

/**************************************************************************/
/*!
   @brief    End a display-writing routine, overwrite in subclasses if
   startWrite is defined!
*/
/**************************************************************************/
void Adafruit_GFX_EndWrite( Adafruit_GFX_Driver_t *driver )
{
	if ( driver->endWrite != NULL )
	{
		driver->endWrite();
	}
}

/**************************************************************************/
/*!
   @brief    Draw a perfectly vertical line (this is often optimized in a
   subclass!)
    @param    x   Top-most x coordinate
    @param    y   Top-most y coordinate
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_DrawFastVLine( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t h,
                                 uint16_t color )
{
	if ( driver->drawFastVLine != NULL )
	{
		driver->drawFastVLine( x, y, h, color );
		return;
	}

	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WriteLine( driver, x, y, x, y + h - 1, color );
	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief    Draw a perfectly horizontal line (this is often optimized in a
   subclass!)
    @param    x   Left-most x coordinate
    @param    y   Left-most y coordinate
    @param    w   Width in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_DrawFastHLine( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w,
                                 uint16_t color )
{
	if ( driver->drawFastHLine != NULL )
	{
		driver->drawFastHLine( x, y, w, color );
		return;
	}

	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WriteLine( driver, x, y, x + w - 1, y, color );
	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief    Fill a rectangle completely with one color. Update in subclasses if
   desired!
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_FillRect( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color )
{
	if ( driver->fillRect != NULL )
	{
		driver->fillRect( x, y, w, h, color );
		return;
	}

	Adafruit_GFX_StartWrite( driver );

	for ( int16_t i = x; i < x + w; i++ )
	{
		Adafruit_GFX_WriteFastVLine( driver, i, y, h, color );
	}

	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief    Fill the screen completely with one color. Update in subclasses if
   desired!
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_FillScreen( Adafruit_GFX_Driver_t *driver, uint16_t color )
{
	if ( driver->fillScreen != NULL )
	{
		driver->fillScreen( color );
		return;
	}

	Adafruit_GFX_FillRect( driver, 0, 0, _width, _height, color );
}

/**************************************************************************/
/*!
   @brief    Draw a line
    @param    x0  Start point x coordinate
    @param    y0  Start point y coordinate
    @param    x1  End point x coordinate
    @param    y1  End point y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawLine( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            uint16_t color )
{
	if ( driver->drawLine != NULL )
	{
		driver->drawLine( x0, y0, x1, y1, color );
		return;
	}

	// Update in subclasses if desired!
	if ( x0 == x1 )
	{
		if ( y0 > y1 )
		{
			_swap_int16_t( y0, y1 );
		}

		Adafruit_GFX_DrawFastVLine( driver, x0, y0, y1 - y0 + 1, color );
	}
	else if ( y0 == y1 )
	{
		if ( x0 > x1 )
		{
			_swap_int16_t( x0, x1 );
		}

		Adafruit_GFX_DrawFastHLine( driver, x0, y0, x1 - x0 + 1, color );
	}
	else
	{
		Adafruit_GFX_StartWrite( driver );
		Adafruit_GFX_WriteLine( driver, x0, y0, x1, y1, color );
		Adafruit_GFX_EndWrite( driver );
	}
}

/**************************************************************************/
/*!
   @brief    Draw a circle outline
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawCircle( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t r,
                              uint16_t color )
{
#if defined(ESP8266)
	yield();
#endif
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WritePixel( driver,  x0, y0 + r, color );
	Adafruit_GFX_WritePixel( driver,  x0, y0 - r, color );
	Adafruit_GFX_WritePixel( driver,  x0 + r, y0, color );
	Adafruit_GFX_WritePixel( driver,  x0 - r, y0, color );

	while ( x < y )
	{
		if ( f >= 0 )
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}

		x++;
		ddF_x += 2;
		f += ddF_x;

		Adafruit_GFX_WritePixel( driver,  x0 + x, y0 + y, color );
		Adafruit_GFX_WritePixel( driver,  x0 - x, y0 + y, color );
		Adafruit_GFX_WritePixel( driver,  x0 + x, y0 - y, color );
		Adafruit_GFX_WritePixel( driver,  x0 - x, y0 - y, color );
		Adafruit_GFX_WritePixel( driver,  x0 + y, y0 + x, color );
		Adafruit_GFX_WritePixel( driver,  x0 - y, y0 + x, color );
		Adafruit_GFX_WritePixel( driver,  x0 + y, y0 - x, color );
		Adafruit_GFX_WritePixel( driver,  x0 - y, y0 - x, color );
	}

	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
    @brief    Quarter-circle drawer, used to do circles and roundrects
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    cornername  Mask bit #1 or bit #2 to indicate which quarters of
   the circle we're doing
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawCircleHelper( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t r,
                                    uint8_t cornername, uint16_t color )
{
	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;

	while ( x < y )
	{
		if ( f >= 0 )
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}

		x++;
		ddF_x += 2;
		f += ddF_x;

		if ( cornername & 0x4 )
		{
			Adafruit_GFX_WritePixel( driver,  x0 + x, y0 + y, color );
			Adafruit_GFX_WritePixel( driver,  x0 + y, y0 + x, color );
		}

		if ( cornername & 0x2 )
		{
			Adafruit_GFX_WritePixel( driver,  x0 + x, y0 - y, color );
			Adafruit_GFX_WritePixel( driver,  x0 + y, y0 - x, color );
		}

		if ( cornername & 0x8 )
		{
			Adafruit_GFX_WritePixel( driver,  x0 - y, y0 + x, color );
			Adafruit_GFX_WritePixel( driver,  x0 - x, y0 + y, color );
		}

		if ( cornername & 0x1 )
		{
			Adafruit_GFX_WritePixel( driver,  x0 - y, y0 - x, color );
			Adafruit_GFX_WritePixel( driver,  x0 - x, y0 - y, color );
		}
	}
}

/**************************************************************************/
/*!
   @brief    Draw a circle with filled color
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_FillCircle( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t r,
                              uint16_t color )
{
	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WriteFastVLine( driver, x0, y0 - r, 2 * r + 1, color );
	Adafruit_GFX_FillCircleHelper( driver, x0, y0, r, 3, 0, color );
	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
    @brief  Quarter-circle drawer with fill, used for circles and roundrects
    @param  x0       Center-point x coordinate
    @param  y0       Center-point y coordinate
    @param  r        Radius of circle
    @param  corners  Mask bits indicating which quarters we're doing
    @param  delta    Offset from center-point, used for round-rects
    @param  color    16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void Adafruit_GFX_FillCircleHelper( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t r,
                                    uint8_t corners, int16_t delta,
                                    uint16_t color )
{

	int16_t f = 1 - r;
	int16_t ddF_x = 1;
	int16_t ddF_y = -2 * r;
	int16_t x = 0;
	int16_t y = r;
	int16_t px = x;
	int16_t py = y;

	delta++; // Avoid some +1's in the loop

	while ( x < y )
	{
		if ( f >= 0 )
		{
			y--;
			ddF_y += 2;
			f += ddF_y;
		}

		x++;
		ddF_x += 2;
		f += ddF_x;

		// These checks avoid double-drawing certain lines, important
		// for the SSD1306 library which has an INVERT drawing mode.
		if ( x < ( y + 1 ) )
		{
			if ( corners & 1 )
			{
				Adafruit_GFX_WriteFastVLine( driver, x0 + x, y0 - y, 2 * y + delta, color );
			}

			if ( corners & 2 )
			{
				Adafruit_GFX_WriteFastVLine( driver, x0 - x, y0 - y, 2 * y + delta, color );
			}
		}

		if ( y != py )
		{
			if ( corners & 1 )
			{
				Adafruit_GFX_WriteFastVLine( driver, x0 + py, y0 - px, 2 * px + delta, color );
			}

			if ( corners & 2 )
			{
				Adafruit_GFX_WriteFastVLine( driver, x0 - py, y0 - px, 2 * px + delta, color );
			}

			py = y;
		}

		px = x;
	}
}

/**************************************************************************/
/*!
   @brief   Draw a rectangle with no fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawRect( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w, int16_t h,
                            uint16_t color )
{
	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WriteFastHLine( driver, x, y, w, color );
	Adafruit_GFX_WriteFastHLine( driver, x, y + h - 1, w, color );
	Adafruit_GFX_WriteFastVLine( driver, x, y, h, color );
	Adafruit_GFX_WriteFastVLine( driver, x + w - 1, y, h, color );
	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief   Draw a rounded rectangle with no fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    r   Radius of corner rounding
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawRoundRect( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w, int16_t h,
                                 int16_t r, uint16_t color )
{
	int16_t max_radius = ( ( w < h ) ? w : h ) / 2; // 1/2 minor axis

	if ( r > max_radius )
	{
		r = max_radius;
	}

	// smarter version
	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WriteFastHLine( driver, x + r, y, w - 2 * r, color );       // Top
	Adafruit_GFX_WriteFastHLine( driver, x + r, y + h - 1, w - 2 * r, color ); // Bottom
	Adafruit_GFX_WriteFastVLine( driver, x, y + r, h - 2 * r, color );       // Left
	Adafruit_GFX_WriteFastVLine( driver, x + w - 1, y + r, h - 2 * r, color ); // Right
	// draw four corners
	Adafruit_GFX_DrawCircleHelper( driver,  x + r, y + r, r, 1, color );
	Adafruit_GFX_DrawCircleHelper( driver,  x + w - r - 1, y + r, r, 2, color );
	Adafruit_GFX_DrawCircleHelper( driver,  x + w - r - 1, y + h - r - 1, r, 4, color );
	Adafruit_GFX_DrawCircleHelper( driver,  x + r, y + h - r - 1, r, 8, color );
	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief   Draw a rounded rectangle with fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    r   Radius of corner rounding
    @param    color 16-bit 5-6-5 Color to draw/fill with
*/
/**************************************************************************/
void Adafruit_GFX_FillRoundRect( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, int16_t w, int16_t h,
                                 int16_t r, uint16_t color )
{
	int16_t max_radius = ( ( w < h ) ? w : h ) / 2; // 1/2 minor axis

	if ( r > max_radius )
	{
		r = max_radius;
	}

	// smarter version
	Adafruit_GFX_StartWrite( driver );
	Adafruit_GFX_WriteFillRect( driver, x + r, y, w - 2 * r, h, color );
	// draw four corners
	Adafruit_GFX_FillCircleHelper( driver, x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color );
	Adafruit_GFX_FillCircleHelper( driver, x + r, y + r, r, 2, h - 2 * r - 1, color );
	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief   Draw a triangle with no fill color
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawTriangle( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                int16_t x2, int16_t y2, uint16_t color )
{
	Adafruit_GFX_DrawLine( driver, x0, y0, x1, y1, color );
	Adafruit_GFX_DrawLine( driver, x1, y1, x2, y2, color );
	Adafruit_GFX_DrawLine( driver, x2, y2, x0, y0, color );
}

/**************************************************************************/
/*!
   @brief     Draw a triangle with color-fill
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to fill/draw with
*/
/**************************************************************************/
void Adafruit_GFX_FillTriangle( Adafruit_GFX_Driver_t *driver, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                int16_t x2, int16_t y2, uint16_t color )
{

	int16_t a, b, y, last;

	// Sort coordinates by Y order (y2 >= y1 >= y0)
	if ( y0 > y1 )
	{
		_swap_int16_t( y0, y1 );
		_swap_int16_t( x0, x1 );
	}

	if ( y1 > y2 )
	{
		_swap_int16_t( y2, y1 );
		_swap_int16_t( x2, x1 );
	}

	if ( y0 > y1 )
	{
		_swap_int16_t( y0, y1 );
		_swap_int16_t( x0, x1 );
	}

	Adafruit_GFX_StartWrite( driver );

	if ( y0 == y2 ) // Handle awkward all-on-same-line case as its own thing
	{
		a = b = x0;

		if ( x1 < a )
		{
			a = x1;
		}
		else if ( x1 > b )
		{
			b = x1;
		}

		if ( x2 < a )
		{
			a = x2;
		}
		else if ( x2 > b )
		{
			b = x2;
		}

		Adafruit_GFX_WriteFastHLine( driver, a, y0, b - a + 1, color );
		Adafruit_GFX_EndWrite( driver );
		return;
	}

	int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
	        dx12 = x2 - x1, dy12 = y2 - y1;
	int32_t sa = 0, sb = 0;

	// For upper part of triangle, find scanline crossings for segments
	// 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
	// is included here (and second loop will be skipped, avoiding a /0
	// error there), otherwise scanline y1 is skipped here and handled
	// in the second loop...which also avoids a /0 error here if y0=y1
	// (flat-topped triangle).
	if ( y1 == y2 )
	{
		last = y1;    // Include y1 scanline
	}
	else
	{
		last = y1 - 1;    // Skip it
	}

	for ( y = y0; y <= last; y++ )
	{
		a = x0 + sa / dy01;
		b = x0 + sb / dy02;
		sa += dx01;
		sb += dx02;

		/* longhand:
		a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/
		if ( a > b )
		{
			_swap_int16_t( a, b );
		}

		Adafruit_GFX_WriteFastHLine( driver, a, y, b - a + 1, color );
	}

	// For lower part of triangle, find scanline crossings for segments
	// 0-2 and 1-2.  This loop is skipped if y1=y2.
	sa = ( int32_t )dx12 * ( y - y1 );
	sb = ( int32_t )dx02 * ( y - y0 );

	for ( ; y <= y2; y++ )
	{
		a = x1 + sa / dy12;
		b = x0 + sb / dy02;
		sa += dx12;
		sb += dx02;

		/* longhand:
		a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
		b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
		*/
		if ( a > b )
		{
			_swap_int16_t( a, b );
		}

		Adafruit_GFX_WriteFastHLine( driver, a, y, b - a + 1, color );
	}

	Adafruit_GFX_EndWrite( driver );
}

// BITMAP / XBITMAP / GRAYSCALE / RGB BITMAP FUNCTIONS ---------------------

/**************************************************************************/
/*!
   @brief      Draw a RAM-resident 1-bit image at the specified (x,y) position,
   using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void Adafruit_GFX_DrawBitmap( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, uint8_t *bitmap, int16_t w,
                              int16_t h, uint16_t color )
{

	int16_t byteWidth = ( w + 7 ) / 8; // Bitmap scanline pad = whole byte
	uint8_t byte = 0;

	Adafruit_GFX_StartWrite( driver );

	for ( int16_t j = 0; j < h; j++, y++ )
	{
		for ( int16_t i = 0; i < w; i++ )
		{
			if ( i & 7 )
			{
				byte <<= 1;
			}
			else
			{
				byte = bitmap[j * byteWidth + i / 8];
			}

			if ( byte & 0x80 )
			{
				Adafruit_GFX_WritePixel( driver,  x + i, y, color );
			}
		}
	}

	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief      Draw a RAM-resident 1-bit image at the specified (x,y) position,
   using the specified foreground (for set bits) and background (unset bits)
   colors.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw pixels with
    @param    bg 16-bit 5-6-5 Color to draw background with
*/
/**************************************************************************/
void Adafruit_GFX_DrawBitmapBg( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, uint8_t *bitmap, int16_t w,
                                int16_t h, uint16_t color, uint16_t bg )
{

	int16_t byteWidth = ( w + 7 ) / 8; // Bitmap scanline pad = whole byte
	uint8_t byte = 0;

	Adafruit_GFX_StartWrite( driver );

	for ( int16_t j = 0; j < h; j++, y++ )
	{
		for ( int16_t i = 0; i < w; i++ )
		{
			if ( i & 7 )
			{
				byte <<= 1;
			}
			else
			{
				byte = bitmap[j * byteWidth + i / 8];
			}

			Adafruit_GFX_WritePixel( driver,  x + i, y, ( byte & 0x80 ) ? color : bg );
		}
	}

	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief   Draw a RAM-resident 8-bit image (grayscale) at the specified (x,y)
   pos. Specifically for 8-bit display devices such as IS31FL3731; no color
   reduction/expansion is performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with grayscale bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void Adafruit_GFX_DrawGrayscaleBitmap( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, uint8_t *bitmap,
                                       int16_t w, int16_t h )
{
	Adafruit_GFX_StartWrite( driver );

	for ( int16_t j = 0; j < h; j++, y++ )
	{
		for ( int16_t i = 0; i < w; i++ )
		{
			Adafruit_GFX_WritePixel( driver,  x + i, y, bitmap[j * w + i] );
		}
	}

	Adafruit_GFX_EndWrite( driver );
}

/**************************************************************************/
/*!
   @brief   Draw a RAM-resident 16-bit image (RGB 5/6/5) at the specified (x,y)
   position. For 16-bit display devices; no color reduction performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with 16-bit color bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void Adafruit_GFX_DrawRGBBitmap( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, uint16_t *bitmap,
                                 int16_t w, int16_t h )
{
	Adafruit_GFX_StartWrite( driver );

	for ( int16_t j = 0; j < h; j++, y++ )
	{
		for ( int16_t i = 0; i < w; i++ )
		{
			Adafruit_GFX_WritePixel( driver,  x + i, y, bitmap[j * w + i] );
		}
	}

	Adafruit_GFX_EndWrite( driver );
}

// TEXT- AND CHARACTER-HANDLING FUNCTIONS ----------------------------------

// Draw a character
/**************************************************************************/
/*!
   @brief   Draw a single character
    @param    x   Bottom left corner x coordinate
    @param    y   Bottom left corner y coordinate
    @param    c   The 8-bit font-indexed character (likely ascii)
    @param    color 16-bit 5-6-5 Color to draw chraracter with
    @param    bg 16-bit 5-6-5 Color to fill background with (if same as color,
   no background)
    @param    size  Font magnification level, 1 is 'original' size
*/
/**************************************************************************/
void Adafruit_GFX_DrawChar( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, unsigned char c,
                            uint16_t color, uint16_t bg, uint8_t size )
{
	Adafruit_GFX_DrawCharAdv( driver, x, y, c, color, bg, size, size );
}

// Draw a character
/**************************************************************************/
/*!
   @brief   Draw a single character
    @param    x   Bottom left corner x coordinate
    @param    y   Bottom left corner y coordinate
    @param    c   The 8-bit font-indexed character (likely ascii)
    @param    color 16-bit 5-6-5 Color to draw chraracter with
    @param    bg 16-bit 5-6-5 Color to fill background with (if same as color,
   no background)
    @param    size_x  Font magnification level in X-axis, 1 is 'original' size
    @param    size_y  Font magnification level in Y-axis, 1 is 'original' size
*/
/**************************************************************************/
void Adafruit_GFX_DrawCharAdv( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, unsigned char c,
                               uint16_t color, uint16_t bg, uint8_t size_x,
                               uint8_t size_y )
{

	if ( !gfxFont ) // 'Classic' built-in font
	{

		if ( ( x >= _width ) ||           // Clip right
		        ( y >= _height ) ||           // Clip bottom
		        ( ( x + 6 * size_x - 1 ) < 0 ) || // Clip left
		        ( ( y + 8 * size_y - 1 ) < 0 ) ) // Clip top
		{
			return;
		}

		if ( !_cp437 && ( c >= 176 ) )
		{
			c++;    // Handle 'classic' charset behavior
		}

		Adafruit_GFX_StartWrite( driver );

		for ( int8_t i = 0; i < 5; i++ ) // Char bitmap = 5 columns
		{
			uint8_t line = pgm_read_byte( &font[c * 5 + i] );

			for ( int8_t j = 0; j < 8; j++, line >>= 1 )
			{
				if ( line & 1 )
				{
					if ( size_x == 1 && size_y == 1 )
					{
						Adafruit_GFX_WritePixel( driver,  x + i, y + j, color );
					}
					else
						Adafruit_GFX_WriteFillRect( driver, x + i * size_x, y + j * size_y, size_x, size_y,
						                            color );
				}
				else if ( bg != color )
				{
					if ( size_x == 1 && size_y == 1 )
					{
						Adafruit_GFX_WritePixel( driver,  x + i, y + j, bg );
					}
					else
					{
						Adafruit_GFX_WriteFillRect( driver, x + i * size_x, y + j * size_y, size_x, size_y, bg );
					}
				}
			}
		}

		if ( bg != color ) // If opaque, draw vertical line for last column
		{
			if ( size_x == 1 && size_y == 1 )
			{
				Adafruit_GFX_WriteFastVLine( driver, x + 5, y, 8, bg );
			}
			else
			{
				Adafruit_GFX_WriteFillRect( driver, x + 5 * size_x, y, size_x, 8 * size_y, bg );
			}
		}

		Adafruit_GFX_EndWrite( driver );

	}
	else     // Custom font
	{

		// Character is assumed previously filtered by write() to eliminate
		// newlines, returns, non-printable characters, etc.  Calling
		// drawChar() directly with 'bad' characters of font may cause mayhem!

		c -= ( uint8_t )pgm_read_byte( &gfxFont->first );
		GFXglyph *glyph = pgm_read_glyph_ptr( gfxFont, c );
		uint8_t *bitmap = pgm_read_bitmap_ptr( gfxFont );

		uint16_t bo = pgm_read_word( &glyph->bitmapOffset );
		uint8_t w = pgm_read_byte( &glyph->width ), h = pgm_read_byte( &glyph->height );
		int8_t xo = pgm_read_byte( &glyph->xOffset ),
		       yo = pgm_read_byte( &glyph->yOffset );
		uint8_t xx, yy, bits = 0, bit = 0;
		int16_t xo16 = 0, yo16 = 0;

		if ( size_x > 1 || size_y > 1 )
		{
			xo16 = xo;
			yo16 = yo;
		}

		// Todo: Add character clipping here

		// NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
		// THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
		// has typically been used with the 'classic' font to overwrite old
		// screen contents with new data.  This ONLY works because the
		// characters are a uniform size; it's not a sensible thing to do with
		// proportionally-spaced fonts with glyphs of varying sizes (and that
		// may overlap).  To replace previously-drawn text when using a custom
		// font, use the getTextBounds() function to determine the smallest
		// rectangle encompassing a string, erase the area with fillRect(),
		// then draw new text.  This WILL infortunately 'blink' the text, but
		// is unavoidable.  Drawing 'background' pixels will NOT fix this,
		// only creates a new set of problems.  Have an idea to work around
		// this (a canvas object type for MCUs that can afford the RAM and
		// displays supporting setAddrWindow() and pushColors()), but haven't
		// implemented this yet.

		Adafruit_GFX_StartWrite( driver );

		for ( yy = 0; yy < h; yy++ )
		{
			for ( xx = 0; xx < w; xx++ )
			{
				if ( !( bit++ & 7 ) )
				{
					bits = pgm_read_byte( &bitmap[bo++] );
				}

				if ( bits & 0x80 )
				{
					if ( size_x == 1 && size_y == 1 )
					{
						Adafruit_GFX_WritePixel( driver,  x + xo + xx, y + yo + yy, color );
					}
					else
					{
						Adafruit_GFX_WriteFillRect( driver, x + ( xo16 + xx ) * size_x, y + ( yo16 + yy ) * size_y,
						                            size_x, size_y, color );
					}
				}

				bits <<= 1;
			}
		}

		Adafruit_GFX_EndWrite( driver );

	} // End classic vs custom font
}
/**************************************************************************/
/*!
    @brief  Print one byte/character of data, used to support print()
    @param  c  The 8-bit ascii character to write
*/
/**************************************************************************/
size_t Adafruit_GFX_Write( Adafruit_GFX_Driver_t *driver, uint8_t c )
{
	if ( !gfxFont ) // 'Classic' built-in font
	{

		if ( c == '\n' )              // Newline?
		{
			cursor_x = 0;               // Reset x to zero,
			cursor_y += textsize_y * 8; // advance y one line
		}
		else if ( c != '\r' )         // Ignore carriage returns
		{
			if ( wrap && ( ( cursor_x + textsize_x * 6 ) > _width ) ) // Off right?
			{
				cursor_x = 0;                                       // Reset x to zero,
				cursor_y += textsize_y * 8; // advance y one line
			}

			Adafruit_GFX_DrawChar( driver, cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x,
			                       textsize_y );
			cursor_x += textsize_x * 6; // Advance x one char
		}

	}
	else     // Custom font
	{

		if ( c == '\n' )
		{
			cursor_x = 0;
			cursor_y +=
			    ( int16_t )textsize_y * ( uint8_t )pgm_read_byte( &gfxFont->yAdvance );
		}
		else if ( c != '\r' )
		{
			uint8_t first = pgm_read_byte( &gfxFont->first );

			if ( ( c >= first ) && ( c <= ( uint8_t )pgm_read_byte( &gfxFont->last ) ) )
			{
				GFXglyph *glyph = pgm_read_glyph_ptr( gfxFont, c - first );
				uint8_t w = pgm_read_byte( &glyph->width ),
				        h = pgm_read_byte( &glyph->height );

				if ( ( w > 0 ) && ( h > 0 ) ) // Is there an associated bitmap?
				{
					int16_t xo = ( int8_t )pgm_read_byte( &glyph->xOffset ); // sic

					if ( wrap && ( ( cursor_x + textsize_x * ( xo + w ) ) > _width ) )
					{
						cursor_x = 0;
						cursor_y += ( int16_t )textsize_y *
						            ( uint8_t )pgm_read_byte( &gfxFont->yAdvance );
					}

					Adafruit_GFX_DrawChar( driver, cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x,
					                       textsize_y );
				}

				cursor_x +=
				    ( uint8_t )pgm_read_byte( &glyph->xAdvance ) * ( int16_t )textsize_x;
			}
		}
	}

	return 1;
}

/**************************************************************************/
/*!
    @brief   Set text 'magnification' size. Each increase in s makes 1 pixel
   that much bigger.
    @param  s  Desired text size. 1 is default 6x8, 2 is 12x16, 3 is 18x24, etc
*/
/**************************************************************************/
void Adafruit_GFX_SetTextSize( Adafruit_GFX_Driver_t *driver, uint8_t s )
{
	textsize_x = ( s > 0 ) ? s : 1;
	textsize_y = ( s > 0 ) ? s : 1;
}

/**************************************************************************/
/*!
    @brief      Set rotation setting for display
    @param  x   0 thru 3 corresponding to 4 cardinal rotations
*/
/**************************************************************************/
void Adafruit_GFX_SetRotation( Adafruit_GFX_Driver_t *driver, uint8_t x )
{
	rotation = ( x & 3 );

	switch ( rotation )
	{
		case 0:
		case 2:
			_width = WIDTH;
			_height = HEIGHT;
			break;

		case 1:
		case 3:
			_width = HEIGHT;
			_height = WIDTH;
			break;
	}
}

/**************************************************************************/
/*!
    @brief Set the font to display when print()ing, either custom or default
    @param  f  The GFXfont object, if NULL use built in 6x8 font
*/
/**************************************************************************/
void Adafruit_GFX_SetFont( Adafruit_GFX_Driver_t *driver, const GFXfont *f )
{
	if ( f )          // Font struct pointer passed in?
	{
		if ( !gfxFont ) // And no current font struct?
		{
			// Switching from classic to new font behavior.
			// Move cursor pos down 6 pixels so it's on baseline.
			cursor_y += 6;
		}
	}
	else if ( gfxFont )   // NULL passed.  Current font struct defined?
	{
		// Switching from new to classic font behavior.
		// Move cursor pos up 6 pixels so it's at top-left of char.
		cursor_y -= 6;
	}

	gfxFont = ( GFXfont * )f;
}

/**************************************************************************/
/*!
    @brief    Helper to determine size of a character with current font/size.
       Broke this out as it's used by both the PROGMEM- and RAM-resident
   getTextBounds() functions.
    @param    c     The ascii character in question
    @param    x     Pointer to x location of character
    @param    y     Pointer to y location of character
    @param    minx  Minimum clipping value for X
    @param    miny  Minimum clipping value for Y
    @param    maxx  Maximum clipping value for X
    @param    maxy  Maximum clipping value for Y
*/
/**************************************************************************/
void Adafruit_GFX_CharBounds( Adafruit_GFX_Driver_t *driver, char c, int16_t *x, int16_t *y, int16_t *minx,
                              int16_t *miny, int16_t *maxx, int16_t *maxy )
{

	if ( gfxFont )
	{

		if ( c == '\n' ) // Newline?
		{
			*x = 0;        // Reset x to zero, advance y by one line
			*y += textsize_y * ( uint8_t )pgm_read_byte( &gfxFont->yAdvance );
		}
		else if ( c != '\r' )   // Not a carriage return; is normal char
		{
			uint8_t first = pgm_read_byte( &gfxFont->first ),
			        last = pgm_read_byte( &gfxFont->last );

			if ( ( c >= first ) && ( c <= last ) ) // Char present in this font?
			{
				GFXglyph *glyph = pgm_read_glyph_ptr( gfxFont, c - first );
				uint8_t gw = pgm_read_byte( &glyph->width ),
				        gh = pgm_read_byte( &glyph->height ),
				        xa = pgm_read_byte( &glyph->xAdvance );
				int8_t xo = pgm_read_byte( &glyph->xOffset ),
				       yo = pgm_read_byte( &glyph->yOffset );

				if ( wrap && ( ( *x + ( ( ( int16_t )xo + gw ) * textsize_x ) ) > _width ) )
				{
					*x = 0; // Reset x to zero, advance y by one line
					*y += textsize_y * ( uint8_t )pgm_read_byte( &gfxFont->yAdvance );
				}

				int16_t tsx = ( int16_t )textsize_x, tsy = ( int16_t )textsize_y,
				        x1 = *x + xo * tsx, y1 = *y + yo * tsy, x2 = x1 + gw * tsx - 1,
				        y2 = y1 + gh * tsy - 1;

				if ( x1 < *minx )
				{
					*minx = x1;
				}

				if ( y1 < *miny )
				{
					*miny = y1;
				}

				if ( x2 > *maxx )
				{
					*maxx = x2;
				}

				if ( y2 > *maxy )
				{
					*maxy = y2;
				}

				*x += xa * tsx;
			}
		}

	}
	else     // Default font
	{

		if ( c == '\n' )        // Newline?
		{
			*x = 0;               // Reset x to zero,
			*y += textsize_y * 8; // advance y one line
			// min/max x/y unchaged -- that waits for next 'normal' character
		}
		else if ( c != '\r' )   // Normal char; ignore carriage returns
		{
			if ( wrap && ( ( *x + textsize_x * 6 ) > _width ) ) // Off right?
			{
				*x = 0;                                       // Reset x to zero,
				*y += textsize_y * 8;                         // advance y one line
			}

			int x2 = *x + textsize_x * 6 - 1, // Lower-right pixel of char
			    y2 = *y + textsize_y * 8 - 1;

			if ( x2 > *maxx )
			{
				*maxx = x2;    // Track max x, y
			}

			if ( y2 > *maxy )
			{
				*maxy = y2;
			}

			if ( *x < *minx )
			{
				*minx = *x;    // Track min x, y
			}

			if ( *y < *miny )
			{
				*miny = *y;
			}

			*x += textsize_x * 6; // Advance x one char
		}
	}
}

/**************************************************************************/
/*!
    @brief    Helper to determine size of a string with current font/size. Pass
   string and a cursor position, returns UL corner and W,H.
    @param    str     The ascii string to measure
    @param    x       The current cursor X
    @param    y       The current cursor Y
    @param    x1      The boundary X coordinate, set by function
    @param    y1      The boundary Y coordinate, set by function
    @param    w      The boundary width, set by function
    @param    h      The boundary height, set by function
*/
/**************************************************************************/
void Adafruit_GFX_GetTextBounds( Adafruit_GFX_Driver_t *driver, const char *str, int16_t x, int16_t y,
                                 int16_t *x1, int16_t *y1, uint16_t *w,
                                 uint16_t *h )
{
	uint8_t c; // Current character

	*x1 = x;
	*y1 = y;
	*w = *h = 0;

	int16_t minx = _width, miny = _height, maxx = -1, maxy = -1;

	while ( ( c = *str++ ) )
	{
		Adafruit_GFX_CharBounds( driver, c, &x, &y, &minx, &miny, &maxx, &maxy );
	}

	if ( maxx >= minx )
	{
		*x1 = minx;
		*w = maxx - minx + 1;
	}

	if ( maxy >= miny )
	{
		*y1 = miny;
		*h = maxy - miny + 1;
	}
}

/**************************************************************************/
/*!
    @brief      Invert the display (ideally using built-in hardware command)
    @param   i  True if you want to invert, false to make 'normal'
*/
/**************************************************************************/
void Adafruit_GFX_InvertDisplay( Adafruit_GFX_Driver_t *driver, bool i )
{
	if ( driver->invertDisplay != NULL )
	{
		driver->invertDisplay( i );
	}
}