/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"
#include <X11/X.h>
#include <X11/Xmd.h>


static Pixmap get_1d_window( unsigned int *width, unsigned int *height );
static Pixmap get_2d_window( unsigned int *width, unsigned int *height );
static Pixmap get_cut_window( unsigned int *width, unsigned int *height );
static void dump_as_ppm( FILE *dp, XImage *image );


/*------------------------------------------------------------------------*/
/* Writes a graphic with the 1D- or 2D-display (if type equals 1 or 2) or */
/* the cross section window (for type equal to 2) to the file descriptor  */
/* passed to the function - the function may throw an exception (instead  */
/* of returning an error code).                                           */
/*------------------------------------------------------------------------*/

void dump_window( int type, int fd )
{
	XImage *image;
	unsigned int w, h;
	Pixmap pm;
	FILE *fp;


	/* With no graphics or without a working hash for the colors no pictures
	   can be send */

	if ( ! G.is_init || ! G.is_fully_drawn || G.color_hash == NULL )
		THROW( EXCEPTION );

	/* We need a stream because write(2) is horribly slow due to it not being
	   buffered */

	if ( ( fp = fdopen( fd, "w" ) ) == NULL )
		THROW( EXCEPTION );

	/* Get a pixmap with the graphic we are supposed to send and convert it
	   to an XImage */

	switch ( type )
	{
		case 1 :
			if ( ! ( G.dim & 1 ) )
				THROW( EXCEPTION );
			pm = get_1d_window( &w, &h );
			break;

		case 2 :
			if ( ! ( G.dim & 2 ) )
				THROW( EXCEPTION );
			pm = get_2d_window( &w, &h );
			break;

		case 3 :
			if ( ! ( G.dim & 2 ) || ! G2.is_cut )
				THROW( EXCEPTION );
			pm = get_cut_window( &w, &h );
			break;

		default :
			THROW( EXCEPTION );
	}

	image = XGetImage( G.d, pm, 0, 0, w, h, AllPlanes, ZPixmap );
	XFreePixmap( G.d, pm );

	if ( image == NULL )
	{
		XDestroyImage( image );
		THROW( EXCEPTION );
	}

	/* Write out the image in PPM format - the web server will have to take
	   care of converting it to a more appropriate format. */

	dump_as_ppm( fp, image );

	XDestroyImage( image );
	fclose( fp );
}


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static Pixmap get_1d_window( unsigned int *width, unsigned int *height )
{
	Pixmap pm;
	GC gc;


	*width  = G1.y_axis.w + G1.canvas.w;
	*height = G1.x_axis.h + G1.canvas.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.run_form_1d->canvas_1d ),
						*width, *height,
						fl_get_canvas_depth( GUI.run_form_1d->canvas_1d ) );
	gc = XCreateGC( G.d, pm, 0, 0 );

	/* Draw the background */

	XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
	XFillRectangle( G.d, pm, gc, 0, 0, *width,*height );

	/* Draw the canvas and both the axis windows */

	XCopyArea( G.d, G1.y_axis.pm, pm, gc, 0, 0, G1.y_axis.w, G1.y_axis.h,
			   0, 0 );
	XCopyArea( G.d, G1.canvas.pm, pm, gc, 0, 0, G1.canvas.w, G1.canvas.h,
			   G1.y_axis.w, 0 );
	XCopyArea( G.d, G1.x_axis.pm, pm, gc, 0, 0, G1.x_axis.w, G1.x_axis.h,
			   G1.y_axis.w, G1.canvas.h );

	/* And finally add a bit of 3D-effect */

	XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
	XDrawLine( G.d, pm, gc, G1.y_axis.w - 1, G1.canvas.h,
			   G1.y_axis.w - 1, *height - 1 );

	XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
	XDrawLine( G.d, pm, gc, 0, G1.y_axis.h,
			   G1.y_axis.w - 1, G1.y_axis.h );

	XFreeGC( G.d, gc );

	return pm;
} 


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static Pixmap get_2d_window( unsigned int *width, unsigned int *height )
{
	Pixmap pm;
	GC gc;


	*width  = G2.y_axis.w + G2.canvas.w + G2.z_axis.w + 5;
	*height = G2.z_axis.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.run_form_2d->canvas_2d ),
						*width, *height,
						fl_get_canvas_depth( GUI.run_form_2d->canvas_2d ) );
	gc = XCreateGC( G.d, pm, 0, 0 );

	/* Draw the background */

	XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
	XFillRectangle( G.d, pm, gc, 0, 0, *width, *height );

	/* Draw the canvas and the three axis windows */

	XCopyArea( G.d, G2.y_axis.pm, pm, gc, 0, 0, G2.y_axis.w, G2.y_axis.h,
			   0, 0 );
	XCopyArea( G.d, G2.canvas.pm, pm, gc, 0, 0, G2.canvas.w, G2.canvas.h,
			   G2.y_axis.w, 0 );
	XCopyArea( G.d, G2.z_axis.pm, pm, gc, 0, 0, G2.z_axis.w, G2.z_axis.h,
			   G2.y_axis.w + G2.canvas.w + 5, 0 );
	XCopyArea( G.d, G2.x_axis.pm, pm, gc, 0, 0, G2.x_axis.w, G2.x_axis.h,
			   G2.y_axis.w, G2.canvas.h );

	/* And finally add a bit of 3D-effect */

	XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
	XDrawLine( G.d, pm, gc, G2.y_axis.w - 1, G2.canvas.h,
			   G2.y_axis.w - 1, *height - 1 );
	XDrawLine( G.d, pm, gc, G2.y_axis.w + G2.canvas.w + 4, 0,
			   G2.y_axis.w + G2.canvas.w + 4, *height - 1 );

	XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
	XDrawLine( G.d, pm, gc, 0, G2.canvas.h,
			   G2.y_axis.w - 1, G2.canvas.h );
	XDrawLine( G.d, pm, gc, G2.y_axis.w + G2.canvas.w, 0,
			   G2.y_axis.w + G2.canvas.w, *height - 1 );

	XFreeGC( G.d, gc );

	return pm;
} 


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static Pixmap get_cut_window( unsigned int *width, unsigned int *height )
{
	Pixmap pm;
	GC gc;


	*width  = G2.cut_y_axis.w + G2.cut_canvas.w + G2.cut_z_axis.w + 5;
	*height = G2.cut_z_axis.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.cut_form->cut_canvas ),
						*width, *height,
						fl_get_canvas_depth( GUI.cut_form->cut_canvas ) );
	gc = XCreateGC( G.d, pm, 0, 0 );

	/* Draw the background */

	XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
	XFillRectangle( G.d, pm, gc, 0, 0, *width, *height );

	/* Draw the canvas and the three axis windows */

	XCopyArea( G.d, G2.cut_y_axis.pm, pm, gc, 0, 0,
			   G2.cut_y_axis.w, G2.cut_y_axis.h, 0, 0 );
	XCopyArea( G.d, G2.cut_canvas.pm, pm, gc, 0, 0,
			   G2.cut_canvas.w, G2.cut_canvas.h, G2.cut_y_axis.w, 0 );
	XCopyArea( G.d, G2.cut_z_axis.pm, pm, gc, 0, 0,
			   G2.cut_z_axis.w, G2.cut_z_axis.h,
			   G2.cut_y_axis.w + G2.cut_canvas.w + 5, 0 );
	XCopyArea( G.d, G2.cut_x_axis.pm, pm, gc, 0, 0,
			   G2.cut_x_axis.w, G2.cut_x_axis.h,
			   G2.cut_y_axis.w, G2.cut_canvas.h );

	/* And finally add a bit of 3D-effect */

	XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
	XDrawLine( G.d, pm, gc, G2.cut_y_axis.w - 1, G2.cut_canvas.h,
			   G2.cut_y_axis.w - 1, *height - 1 );
	XDrawLine( G.d, pm, gc, G2.cut_y_axis.w + G2.cut_canvas.w + 4, 0,
			   G2.cut_y_axis.w + G2.cut_canvas.w + 4, *height - 1 );

	XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
	XDrawLine( G.d, pm, gc, 0, G2.cut_y_axis.h,
			   G2.cut_y_axis.w - 1, G2.cut_y_axis.h );
	XDrawLine( G.d, pm, gc, G2.cut_y_axis.w + G2.cut_canvas.w, 0,
			   G2.cut_y_axis.w + G2.cut_canvas.w, *height - 1 );

	XFreeGC( G.d, gc );

	return pm;
}


/*----------------------------------------------------------------*/
/* This function determines the pixel values of an image and uses */
/* a hash created earlier to figure out the color of the points.  */
/* It then writes out the colors as three rgb byte values to a    */
/* file.                                                          */
/* The part for determining the pixel values (instead of using    */
/* XGetPixel(), which would be much slower) includes some ideas   */
/* from the xv program (notably xvgrab.c) by John Bradley, which  */
/* in turn seems to be based on 'xwdtopnm.c', which is part of    */
/* the pbmplus package written by Jef Poskanzer.                  */
/*----------------------------------------------------------------*/

static void dump_as_ppm( FILE *fp, XImage *image )
{
	int i, j;
	unsigned long pixel = 0;
	unsigned long pixel_mask;
	int key;
	int bits_used;
	int bits_per_item;
	int bits_per_pixel;
	int bit_order;
	int bit_shift = 0;
	CARD8  *bptr;
	CARD16 *sptr;
	CARD32 *lptr;
	char *lineptr;
	G_Hash hash;
	unsigned int hash_size;


	hash = G.color_hash;
	hash_size = G.color_hash_size;

	fprintf( fp, "P6\n%d %d\n255\n", image->width, image->height );

	/* Get some information about the image */

	bits_used = bits_per_item = image->bitmap_unit;
	bits_per_pixel = image->bits_per_pixel;

	if ( bits_per_pixel == 32 )
		pixel_mask = ~ ( ( unsigned long ) 0 );
	else 
		pixel_mask = ( ( ( unsigned long ) 1 ) << bits_per_pixel ) - 1;

	bit_order = image->bitmap_bit_order;

	/* Loop through the complete image, line by line */

	for ( lineptr = image->data, i = 0; i < image->height;
		  lineptr += image->bytes_per_line, i++ )
	{
		bptr = ( ( CARD8  * ) lineptr ) - 1;
		sptr = ( ( CARD16 * ) lineptr ) - 1;
		lptr = ( ( CARD32 * ) lineptr ) - 1;

		bits_used = bits_per_item;

		/* Get next pixel in current line */

		for ( j = 0; j < image->width; j++ )
		{
			if ( bits_used == bits_per_item )
			{  
				bptr++;
				sptr++;
				lptr++;
		   
				bits_used = 0;

				if ( bit_order == MSBFirst )
					bit_shift = bits_per_item - bits_per_pixel;
				else
					bit_shift = 0;
			}

			switch ( bits_per_item )
			{
				case 8 :
					pixel = ( *bptr >> bit_shift ) & pixel_mask;
					break;

				case 16 :
					pixel = ( *sptr >> bit_shift ) & pixel_mask;
					break;

				case 32 :
					pixel = ( *lptr >> bit_shift ) & pixel_mask;
					break;
			}

			if ( bit_order == MSBFirst )
				bit_shift -= bits_per_pixel;
			else
				bit_shift += bits_per_pixel;

			bits_used += bits_per_pixel;

			/* Get the rgb color from the pixel value via a hash look-up
			   and write color to output file */

			key = pixel % hash_size;
			while ( hash[ key ].pixel != pixel )
				key = ( key + 1 ) % hash_size;
			fwrite( hash[ key ].rgb, 1, 3, fp );
		}
	}
}


/*-----------------------------------------------------------------------*/
/* Creates a pixel value to rgb color hash for all colors possibly used. */
/*-----------------------------------------------------------------------*/

void create_color_hash( void )
{
	FL_COLOR i;
	FL_COLOR pixel;
	int key;
	int r = 0, g = 0, b = 0;
	unsigned int hash_size = COLOR_HASH_SIZE;
	G_Hash hash;


	TRY
	{
		hash = G_HASH_ENTRY_P T_malloc( hash_size * sizeof( G_Hash_Entry ) );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		G.color_hash = NULL;
		return;
	}

	for ( i = 0; i < hash_size; i++ )
		hash[ i ].is_used = UNSET;

	for ( i = 0; i < NUM_COLORS + FL_FREE_COL1 + 2; i++ )
	{
		pixel = fl_get_pixel( i );

		key = pixel % hash_size;
		while ( hash[ key ].is_used )
			key = ( key + 1 ) % hash_size;

		hash[ key ].is_used = SET;
		hash[ key ].pixel = pixel;

		fl_getmcolor( i , &r, &g, &b );

		hash[ key ].rgb[ RED   ] = r;
		hash[ key ].rgb[ GREEN ] = g;
		hash[ key ].rgb[ BLUE  ] = b;
	}

	G.color_hash = hash;
	G.color_hash_size = hash_size;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
