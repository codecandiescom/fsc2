/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

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


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

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

	/* We need a stream because write(2) is horribly slow because it does
	   not use buffering */

	if ( ( fp = fdopen( fd, "w" ) ) == NULL )
		THROW( EXCEPTION );

	/* Get a pixmap with the garphic we are supposed to send and convert it
	   to an XImage */

	if ( type == 1 )
	{
		if ( G.dim == 1 )
			pm = get_1d_window( &w, &h );
		else
			pm = get_2d_window( &w, &h );
	}
	else
	{
		if ( ! G.is_cut )
			THROW( EXCEPTION );
		pm = get_cut_window( &w, &h );
	}

	image = XGetImage( G.d, pm, 0, 0, w, h, AllPlanes, ZPixmap );
	XFreePixmap( G.d, pm );

	if ( image == NULL )
	{
		XDestroyImage( image );
		THROW( EXCEPTION );
	}

	/* Write out the image in PPM format - the webserver will have to take
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


	*width  = G.y_axis.w + G.canvas.w;
	*height = G.x_axis.h + G.canvas.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.run_form->canvas ),
						*width, *height,
						fl_get_canvas_depth( GUI.run_form->canvas ) );
	gc = XCreateGC( G.d, pm, 0, 0 );

	/* Draw the background */

	XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
	XFillRectangle( G.d, pm, gc, 0, 0, *width,*height );

	/* Draw the canvas and both the axis windows */

	XCopyArea( G.d, G.y_axis.pm, pm, gc, 0, 0, G.y_axis.w, G.y_axis.h, 0, 0 );
	XCopyArea( G.d, G.canvas.pm, pm, gc, 0, 0, G.canvas.w, G.canvas.h,
			   G.y_axis.w, 0 );
	XCopyArea( G.d, G.x_axis.pm, pm, gc, 0, 0, G.x_axis.w, G.x_axis.h,
			   G.y_axis.w, G.canvas.h );

	/* And finally add a bit of 3D-effect */

	XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
	XDrawLine( G.d, pm, gc, G.y_axis.w - 1, G.canvas.h,
			   G.y_axis.w - 1, *height - 1 );

	XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
	XDrawLine( G.d, pm, gc, 0, G.cut_y_axis.h,
			   G.y_axis.w - 1, G.cut_y_axis.h );

	XFreeGC( G.d, gc );

	return pm;
} 


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static Pixmap get_2d_window( unsigned int *width, unsigned int *height )
{
	Pixmap pm;
	GC gc;


	*width  = G.y_axis.w + G.canvas.w + G.z_axis.w + 5;
	*height = G.z_axis.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.run_form->canvas ),
						*width, *height,
						fl_get_canvas_depth( GUI.run_form->canvas ) );
	gc = XCreateGC( G.d, pm, 0, 0 );

	/* Draw the background */

	XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
	XFillRectangle( G.d, pm, gc, 0, 0, *width, *height );

	/* Draw the canvas and the three axis windows */

	XCopyArea( G.d, G.y_axis.pm, pm, gc, 0, 0, G.y_axis.w, G.y_axis.h, 0, 0 );
	XCopyArea( G.d, G.canvas.pm, pm, gc, 0, 0, G.canvas.w, G.canvas.h,
			   G.y_axis.w, 0 );
	XCopyArea( G.d, G.z_axis.pm, pm, gc, 0, 0, G.z_axis.w, G.z_axis.h,
			   G.y_axis.w + G.canvas.w + 5, 0 );
	XCopyArea( G.d, G.x_axis.pm, pm, gc, 0, 0, G.x_axis.w, G.x_axis.h,
			   G.y_axis.w, G.canvas.h );

	/* And finally add a bit of 3D-effect */

	XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
	XDrawLine( G.d, pm, gc, G.y_axis.w - 1, G.canvas.h,
			   G.y_axis.w - 1, *height - 1 );
	XDrawLine( G.d, pm, gc, G.y_axis.w + G.canvas.w + 4, 0,
			   G.y_axis.w + G.canvas.w + 4, *height - 1 );

	XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
	XDrawLine( G.d, pm, gc, 0, G.canvas.h,
			   G.y_axis.w - 1, G.canvas.h );
	XDrawLine( G.d, pm, gc, G.y_axis.w + G.canvas.w, 0,
			   G.y_axis.w + G.canvas.w, *height - 1 );

	XFreeGC( G.d, gc );

	return pm;
} 


/*---------------------------------------------------------------*/
/*---------------------------------------------------------------*/

static Pixmap get_cut_window( unsigned int *width, unsigned int *height )
{
	Pixmap pm;
	GC gc;


	*width  = G.cut_y_axis.w + G.cut_canvas.w + G.cut_z_axis.w + 5;
	*height = G.cut_z_axis.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.cut_form->cut_canvas ),
						*width, *height,
						fl_get_canvas_depth( GUI.cut_form->cut_canvas ) );
	gc = XCreateGC( G.d, pm, 0, 0 );

	/* Draw the background */

	XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
	XFillRectangle( G.d, pm, gc, 0, 0, *width, *height );

	/* Draw the canvas and the three axis windows */

	XCopyArea( G.d, G.cut_y_axis.pm, pm, gc, 0, 0,
			   G.cut_y_axis.w, G.cut_y_axis.h, 0, 0 );
	XCopyArea( G.d, G.cut_canvas.pm, pm, gc, 0, 0,
			   G.cut_canvas.w, G.cut_canvas.h, G.cut_y_axis.w, 0 );
	XCopyArea( G.d, G.cut_z_axis.pm, pm, gc, 0, 0,
			   G.cut_z_axis.w, G.cut_z_axis.h,
			   G.cut_y_axis.w + G.cut_canvas.w + 5, 0 );
	XCopyArea( G.d, G.cut_x_axis.pm, pm, gc, 0, 0,
			   G.cut_x_axis.w, G.cut_x_axis.h,
			   G.cut_y_axis.w, G.cut_canvas.h );

	/* And finally add a bit of 3D-effect */

	XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
	XDrawLine( G.d, pm, gc, G.cut_y_axis.w - 1, G.cut_canvas.h,
			   G.cut_y_axis.w - 1, *height - 1 );
	XDrawLine( G.d, pm, gc, G.cut_y_axis.w + G.cut_canvas.w + 4, 0,
			   G.cut_y_axis.w + G.cut_canvas.w + 4, *height - 1 );

	XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
	XDrawLine( G.d, pm, gc, 0, G.cut_y_axis.h,
			   G.cut_y_axis.w - 1, G.cut_y_axis.h );
	XDrawLine( G.d, pm, gc, G.cut_y_axis.w + G.cut_canvas.w, 0,
			   G.cut_y_axis.w + G.cut_canvas.w, *height - 1 );

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
/* from the xv program (especially xvgrab.c) by John Bradley,     */
/* which in turn seems to be based on the 'xwdtopnm.c' utility,   */
/* part of the pbmplus package written by Jef Poskanzer.          */
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

	bits_used = bits_per_item  = image->bitmap_unit;
	bits_per_pixel = image->bits_per_pixel;

	if ( bits_per_pixel == 32 )
		pixel_mask = ~ ( ( unsigned long ) 0 );
	else 
		pixel_mask = ( ( ( unsigned long ) 1 ) << bits_per_pixel ) - 1;

	bit_order  = image->bitmap_bit_order;

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
		hash = T_malloc( hash_size * sizeof( G_Hash_Entry ) );
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

		fl_getmcolor( ( FL_COLOR ) i , &r, &g, &b );

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
