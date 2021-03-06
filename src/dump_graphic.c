/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"
#include <X11/X.h>
#include <X11/Xmd.h>


static Pixmap get_1d_window( unsigned int * width,
                             unsigned int * height );
static Pixmap get_2d_window( unsigned int * width,
                             unsigned int * height );
static Pixmap get_cut_window( unsigned int * width,
                              unsigned int * height );
static void dump_as_ppm( FILE   * dp,
                         XImage * image );


/*------------------------------------------------------------------------*
 * Writes a graphic with the 1D- or 2D-display (if type equals 1 or 2) or
 * the cross section window (for type equal to 2) to the file descriptor
 * passed to the function - the function may throw an exception (instead
 * of returning an error code).
 *------------------------------------------------------------------------*/

void
dump_window( int type,
             int fd )
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
            if ( ! ( G.dim & 2 ) || ! G_2d.is_cut )
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


/*--------------------------------------------------------------*
 * Function creates a pixmap with a copy of the contents of the
 * canvas for drawing 1D data and the surrounding axes canvases
 *--------------------------------------------------------------*/

static Pixmap
get_1d_window( unsigned int * width,
               unsigned int * height )
{
    Pixmap pm;
    GC gc;


    *width  = G_1d.y_axis.w + G_1d.canvas.w;
    *height = G_1d.x_axis.h + G_1d.canvas.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.run_form_1d->canvas_1d ),
                        *width, *height,
                        fl_get_canvas_depth( GUI.run_form_1d->canvas_1d ) );
    gc = XCreateGC( G.d, pm, 0, NULL );

    /* Draw the background */

    XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
    XFillRectangle( G.d, pm, gc, 0, 0, *width,*height );

    /* Draw the canvas and both the axis windows */

    XCopyArea( G.d, G_1d.y_axis.pm, pm, gc, 0, 0, G_1d.y_axis.w, G_1d.y_axis.h,
               0, 0 );
    XCopyArea( G.d, G_1d.canvas.pm, pm, gc, 0, 0, G_1d.canvas.w, G_1d.canvas.h,
               G_1d.y_axis.w, 0 );
    XCopyArea( G.d, G_1d.x_axis.pm, pm, gc, 0, 0, G_1d.x_axis.w, G_1d.x_axis.h,
               G_1d.y_axis.w, G_1d.canvas.h );

    /* And finally add a bit of 3D-effect */

    XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
    XDrawLine( G.d, pm, gc, G_1d.y_axis.w - 1, G_1d.canvas.h,
               G_1d.y_axis.w - 1, *height - 1 );

    XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
    XDrawLine( G.d, pm, gc, 0, G_1d.y_axis.h,
               G_1d.y_axis.w - 1, G_1d.y_axis.h );

    XFreeGC( G.d, gc );

    return pm;
}


/*--------------------------------------------------------------*
 * Function creates a pixmap with a copy of the contents of the
 * canvas for drawing 2D data and the surrounding axes canvases
 *--------------------------------------------------------------*/

static Pixmap
get_2d_window( unsigned int * width,
               unsigned int * height )
{
    Pixmap pm;
    GC gc;


    *width  = G_2d.y_axis.w + G_2d.canvas.w + G_2d.z_axis.w + 5;
    *height = G_2d.z_axis.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.run_form_2d->canvas_2d ),
                        *width, *height,
                        fl_get_canvas_depth( GUI.run_form_2d->canvas_2d ) );
    gc = XCreateGC( G.d, pm, 0, NULL );

    /* Draw the background */

    XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
    XFillRectangle( G.d, pm, gc, 0, 0, *width, *height );

    /* Draw the canvas and the three axis windows */

    XCopyArea( G.d, G_2d.y_axis.pm, pm, gc, 0, 0, G_2d.y_axis.w, G_2d.y_axis.h,
               0, 0 );
    XCopyArea( G.d, G_2d.canvas.pm, pm, gc, 0, 0, G_2d.canvas.w, G_2d.canvas.h,
               G_2d.y_axis.w, 0 );
    XCopyArea( G.d, G_2d.z_axis.pm, pm, gc, 0, 0, G_2d.z_axis.w, G_2d.z_axis.h,
               G_2d.y_axis.w + G_2d.canvas.w + 5, 0 );
    XCopyArea( G.d, G_2d.x_axis.pm, pm, gc, 0, 0, G_2d.x_axis.w, G_2d.x_axis.h,
               G_2d.y_axis.w, G_2d.canvas.h );

    /* And finally add a bit of 3D-effect */

    XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
    XDrawLine( G.d, pm, gc, G_2d.y_axis.w - 1, G_2d.canvas.h,
               G_2d.y_axis.w - 1, *height - 1 );
    XDrawLine( G.d, pm, gc, G_2d.y_axis.w + G_2d.canvas.w + 4, 0,
               G_2d.y_axis.w + G_2d.canvas.w + 4, *height - 1 );

    XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
    XDrawLine( G.d, pm, gc, 0, G_2d.canvas.h,
               G_2d.y_axis.w - 1, G_2d.canvas.h );
    XDrawLine( G.d, pm, gc, G_2d.y_axis.w + G_2d.canvas.w, 0,
               G_2d.y_axis.w + G_2d.canvas.w, *height - 1 );

    XFreeGC( G.d, gc );

    return pm;
}


/*---------------------------------------------------------------------*
 * Function creates a pixmap with a copy of the contents of the canvas
 * for cross sections through a 2D data set and the surrounding axes
 * canvases
 *---------------------------------------------------------------------*/

static Pixmap
get_cut_window( unsigned int * width,
                unsigned int * height )
{
    Pixmap pm;
    GC gc;


    *width  = G_2d.cut_y_axis.w + G_2d.cut_canvas.w + G_2d.cut_z_axis.w + 5;
    *height = G_2d.cut_z_axis.h;

    pm = XCreatePixmap( G.d, FL_ObjWin( GUI.cut_form->cut_canvas ),
                        *width, *height,
                        fl_get_canvas_depth( GUI.cut_form->cut_canvas ) );
    gc = XCreateGC( G.d, pm, 0, NULL );

    /* Draw the background */

    XSetForeground( G.d, gc, fl_get_pixel( FL_COL1 ) );
    XFillRectangle( G.d, pm, gc, 0, 0, *width, *height );

    /* Draw the canvas and the three axis windows */

    XCopyArea( G.d, G_2d.cut_y_axis.pm, pm, gc, 0, 0,
               G_2d.cut_y_axis.w, G_2d.cut_y_axis.h, 0, 0 );
    XCopyArea( G.d, G_2d.cut_canvas.pm, pm, gc, 0, 0,
               G_2d.cut_canvas.w, G_2d.cut_canvas.h, G_2d.cut_y_axis.w, 0 );
    XCopyArea( G.d, G_2d.cut_z_axis.pm, pm, gc, 0, 0,
               G_2d.cut_z_axis.w, G_2d.cut_z_axis.h,
               G_2d.cut_y_axis.w + G_2d.cut_canvas.w + 5, 0 );
    XCopyArea( G.d, G_2d.cut_x_axis.pm, pm, gc, 0, 0,
               G_2d.cut_x_axis.w, G_2d.cut_x_axis.h,
               G_2d.cut_y_axis.w, G_2d.cut_canvas.h );

    /* And finally add a bit of 3D-effect */

    XSetForeground( G.d, gc, fl_get_pixel( FL_BLACK ) );
    XDrawLine( G.d, pm, gc, G_2d.cut_y_axis.w - 1, G_2d.cut_canvas.h,
               G_2d.cut_y_axis.w - 1, *height - 1 );
    XDrawLine( G.d, pm, gc, G_2d.cut_y_axis.w + G_2d.cut_canvas.w + 4, 0,
               G_2d.cut_y_axis.w + G_2d.cut_canvas.w + 4, *height - 1 );

    XSetForeground( G.d, gc, fl_get_pixel( FL_LEFT_BCOL ) );
    XDrawLine( G.d, pm, gc, 0, G_2d.cut_y_axis.h,
               G_2d.cut_y_axis.w - 1, G_2d.cut_y_axis.h );
    XDrawLine( G.d, pm, gc, G_2d.cut_y_axis.w + G_2d.cut_canvas.w, 0,
               G_2d.cut_y_axis.w + G_2d.cut_canvas.w, *height - 1 );

    XFreeGC( G.d, gc );

    return pm;
}


/*----------------------------------------------------------------*
 * This function determines the pixel values of an image and uses
 * a hash created earlier to figure out the color of the points.
 * It then writes out the colors as three rgb byte values to a
 * file.
 * The part for determining the pixel values (instead of using
 * XGetPixel(), which would be much slower) includes some ideas
 * from the xv program (notably xvgrab.c) by John Bradley, which
 * in turn seems to be based on 'xwdtopnm.c', which is part of
 * the pbmplus package written by Jef Poskanzer.
 *----------------------------------------------------------------*/

static void
dump_as_ppm( FILE   * fp,
             XImage * image )
{
    G_Hash_Entry_T * hash = G.color_hash;
    unsigned int  hash_size = G.color_hash_size;

    fprintf( fp, "P6\n%d %d\n255\n", image->width, image->height );

    for ( int i = 0; i < image->height; i++ )
        for ( int j = 0; j < image->width; j++ )
        {
            unsigned long pixel = XGetPixel( image, j, i );
            int key = pixel % hash_size;

            if ( ! hash[ key ].is_used )
            {
                // This can happen due to TTF anti-alializing! Not
                // a clean solution (since it expects a certain way
                // the color is coded into the pxiel) but better than
                // nothing for the time being...

                hash[ key ].rgb[ RED   ] = pixel >> 16 & 0xFF;
                hash[ key ].rgb[ GREEN ] = pixel >> 16 & 0xFF;
                hash[ key ].rgb[ BLUE  ] =  pixel      & 0xFF;
            }
            else
                while ( hash[ key ].pixel != pixel )
                {
                    key = ( key + 1 ) % hash_size;
                    if ( ! hash[ key ].is_used )
                    {
                        hash[ key ].rgb[ RED   ] = pixel >> 16 & 0xFF;
                        hash[ key ].rgb[ GREEN ] = pixel >> 16 & 0xFF;
                        hash[ key ].rgb[ BLUE  ] =  pixel      & 0xFF;
                        break;
                    }
                }

            fwrite( hash[ key ].rgb, 1, 3, fp );
        }
}


/*-----------------------------------------------------------------------*
 * Creates a pixel value to rgb color hash for all colors possibly used.
 *-----------------------------------------------------------------------*/

void
create_color_hash( void )
{
    unsigned int hash_size = COLOR_HASH_SIZE;
    G_Hash_Entry_T * hash;


    TRY
    {
        hash = T_malloc( hash_size * sizeof *hash );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        G.color_hash = NULL;
        G.color_hash_size = 0;
        return;
    }

    for ( unsigned int i = 0; i < hash_size; i++ )
        hash[ i ].is_used = false;

    FL_COLOR i = 0;
    for ( i = 0; i < FL_FREE_COL1 + NUM_COLORS + 2; i++ )
    {
        FL_COLOR pixel = fl_get_pixel( i );

        int r, g, b;
        pixel = fl_getmcolor( i , &r, &g, &b );

        FL_COLOR key = pixel % hash_size;
        while ( hash[ key ].is_used )
            key = ( key + 1 ) % hash_size;

        hash[ key ].is_used = true;
        hash[ key ].pixel = pixel;

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
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
