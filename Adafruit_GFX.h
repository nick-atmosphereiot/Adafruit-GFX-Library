#ifndef _ADAFRUIT_GFX_H
#define _ADAFRUIT_GFX_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "gfxfont.h"

typedef struct
{
	void ( *drawPixel )( int16_t x, int16_t y, uint16_t color );

	// Any functions below this are OPTIONAL. You can choose to override, but otherwise
	// the default implementation will be used.
	void ( *startWrite )();
	void ( *writePixel )( int16_t x, int16_t y, uint16_t color );
	void ( *writeFillRect )( int16_t x, int16_t y, int16_t w, int16_t h,
	                         uint16_t color );
	void ( *writeFastVLine )( int16_t x, int16_t y, int16_t h, uint16_t color );
	void ( *writeFastHLine )( int16_t x, int16_t y, int16_t w, uint16_t color );
	void ( *writeLine )( int16_t x0, int16_t y0, int16_t x1, int16_t y1,
	                     uint16_t color );
	void ( *endWrite )( void );
	void ( *setRotation )( uint8_t r );
	void ( *invertDisplay )( bool i );
	void ( *drawFastVLine )( int16_t x, int16_t y, int16_t h, uint16_t color );
	void ( *drawFastHLine )( int16_t x, int16_t y, int16_t w, uint16_t color );
	void ( *fillRect )( int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color );
	void ( *fillScreen )( uint16_t color );
	void ( *drawLine )( int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color );
	void ( *drawRect )( int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color );
} Adafruit_GFX_Driver_t;

void Adafruit_GFX_Init( Adafruit_GFX_Driver_t *driver, int16_t w, int16_t h );
void Adafruit_GFX_DrawCircle( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t r, uint16_t color );
void Adafruit_GFX_DrawCircleHelper( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color );
void Adafruit_GFX_FillCircle( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t r, uint16_t color );
void Adafruit_GFX_FillCircleHelper( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color );
void Adafruit_GFX_DrawTriangle( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color );
void Adafruit_GFX_FillTriangle( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color );
void Adafruit_GFX_DrawRoundRect( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color );
void Adafruit_GFX_FillRoundRect( Adafruit_GFX_Driver_t *driver,  int16_t x0, int16_t y0, int16_t w, int16_t h, int16_t radius, uint16_t color );
void Adafruit_GFX_DrawBitmap( Adafruit_GFX_Driver_t *driver,  int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color );
void Adafruit_GFX_DrawBitmapBg( Adafruit_GFX_Driver_t *driver,  int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg );
void Adafruit_GFX_DrawGrayscaleBitmap( Adafruit_GFX_Driver_t *driver,  int16_t x, int16_t y, uint8_t *bitmap, int16_t w,  int16_t h );
void Adafruit_GFX_DrawRGBBitmap( Adafruit_GFX_Driver_t *driver,  int16_t x, int16_t y, uint16_t *bitmap, int16_t w,  int16_t h );
void Adafruit_GFX_DrawChar( Adafruit_GFX_Driver_t *driver,  int16_t x, int16_t y, unsigned char c, uint16_t color,   uint16_t bg, uint8_t size_x, uint8_t size_y );
void Adafruit_GFX_DrawCharAdv( Adafruit_GFX_Driver_t *driver,  int16_t x, int16_t y, unsigned char c, uint16_t color,   uint16_t bg, uint8_t size );
void Adafruit_GFX_GetTextBounds( Adafruit_GFX_Driver_t *driver,  const char *string, int16_t x, int16_t y, int16_t *x1,  int16_t *y1, uint16_t *w, uint16_t *h );
void Adafruit_GFX_SetTextSize( Adafruit_GFX_Driver_t *driver,  uint8_t s );
void Adafruit_GFX_SetFont( Adafruit_GFX_Driver_t *driver,  const GFXfont *f );
void Adafruit_GFX_SetCursor( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y );
void Adafruit_GFX_SetTextColor( Adafruit_GFX_Driver_t *driver, uint16_t c );
void Adafruit_GFX_SetTextColorWithBg( Adafruit_GFX_Driver_t *driver, uint16_t c, uint16_t bg );
void Adafruit_GFX_SetTextWrap( Adafruit_GFX_Driver_t *driver, bool w );
size_t Adafruit_GFX_Write( Adafruit_GFX_Driver_t *driver, uint8_t c );
int16_t Adafruit_GFX_Width( Adafruit_GFX_Driver_t *driver );
int16_t Adafruit_GFX_Height( Adafruit_GFX_Driver_t *driver );
uint8_t Adafruit_GFX_GetRotation( Adafruit_GFX_Driver_t *driver );
int16_t Adafruit_GFX_GetCursorX( Adafruit_GFX_Driver_t *driver );
int16_t Adafruit_GFX_GetCursorY(Adafruit_GFX_Driver_t *driver);

	void Adafruit_GFX_StartWrite(Adafruit_GFX_Driver_t *driver);
	void Adafruit_GFX_WritePixel( Adafruit_GFX_Driver_t *driver, int16_t x, int16_t y, uint16_t color );
	void Adafruit_GFX_WriteFillRect( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t w, int16_t h,
	                         uint16_t color );
	void Adafruit_GFX_WriteFastVLine( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t h, uint16_t color );
	void Adafruit_GFX_WriteFastHLine( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t w, uint16_t color );
	void Adafruit_GFX_WriteLine( Adafruit_GFX_Driver_t *driver,int16_t x0, int16_t y0, int16_t x1, int16_t y1,
	                     uint16_t color );
	void Adafruit_GFX_EndWrite( Adafruit_GFX_Driver_t *driver, );
	void Adafruit_GFX_SetRotation( Adafruit_GFX_Driver_t *driver,uint8_t r );
	void Adafruit_GFX_InvertDisplay( Adafruit_GFX_Driver_t *driver,bool i );
	void Adafruit_GFX_DrawFastVLine( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t h, uint16_t color );
	void Adafruit_GFX_DrawFastHLine( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t w, uint16_t color );
	void Adafruit_GFX_FillRect( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color );
	void Adafruit_GFX_FillScreen( Adafruit_GFX_Driver_t *driver,uint16_t color );
	void Adafruit_GFX_DrawLine( Adafruit_GFX_Driver_t *driver,int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color );
	void Adafruit_GFX_DrawRect( Adafruit_GFX_Driver_t *driver,int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color );

#endif // _ADAFRUIT_GFX_H
