/*
  $Id$

  Copyright (c) 2001 Jens Thoms Toerring

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


#include "tds540.h"


static void tds540_window_check_1( bool *is_start, bool *is_width );
static void tds540_window_check_2( void );
static void tds540_window_check_3( void );


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

const char *tds540_ptime( double time )
{
	static char buffer[ 128 ];


	if ( fabs( time ) >= 1.0 )
		sprintf( buffer, "%g s", time );
	else if ( fabs( time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * time );
	else if ( fabs( time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * time );
	else
		sprintf( buffer, "%g ns", 1.e9 * time );

	return buffer;
}


/*-----------------------------------------------------------------*/
/* Deletes a window by removing it from the linked list of windows */
/*-----------------------------------------------------------------*/

void tds540_delete_windows( void )
{
	WINDOW *w;


	while ( tds540.w != NULL )
	{
		w = tds540.w;
		tds540.w = w->next;
		T_free( w );
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds540_do_pre_exp_checks( void )
{
	WINDOW *w;
	bool is_start, is_width;
	double width;
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds540.is_trigger_channel )
		tds540_set_trigger_channel( 
			Channel_Names[ tds540.trigger_channel ] );

	/* Switch all channels on that get used in the measurements */

	for ( i = 0; i <= TDS540_REF4; i++)
		if ( tds540.channels_in_use[ i ] )
			tds540_display_channel( i );

	/* Remove all unused windows and test if for all other windows the start
	   position and the width is set */

	tds540_window_check_1( &is_start, &is_width );

	/* That's all if no windows have been defined - we switch off gated
	   measurement mode, i.e. all measurement operations are done on the
	   whole curve */

	if ( tds540.w == NULL )
	{
		tds540_set_gated_meas( UNSET );
		tds540.gated_state = UNSET;
		return;
	}

	/* If start position isn't set for all windows set it to the position of
	   the left cursor */

	if ( ! is_start )
		for ( w = tds540.w; w != NULL; w = w->next )
			if ( ! w->is_start )
			{
				w->start = tds540.cursor_pos;
				w->is_start = SET;
			}

	/* If the width wasn't defined for all windows get the distance of the
	   cursors on the screen and use it as the default width */

	if ( ! is_width )
	{
		tds540_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			eprint( FATAL, UNSET, "%s: Can't determine a reasonable value for "
					"the missing window widths.\n", DEVICE_NAME );
			THROW( EXCEPTION );
		}

		for ( w = tds540.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursorsd can be positioned exactly
	   and are still within the range of the digitizers record length */

	tds540_window_check_2( );
	tds540_window_check_3( );

	/* Now that all windows are properly set we switch on gated measurements */

	tds540_set_gated_meas( SET );
	tds540.gated_state = SET;

	/* If the widths of all windows are equal we switch on tracking cursor
	   mode and set the cursors to the position of the first window */

	if ( tds540.is_equal_width )
	{
		tds540_set_cursor( 1, tds540.w->start );
		tds540_set_cursor( 2, tds540.w->start + tds540.w->width );
		tds540_set_track_cursors( SET );
		tds540.cursor_pos = tds540.w->start;
	}
	else
		tds540_set_track_cursors( UNSET );
}


/*---------------------------------------------------------------*/
/* Removes unused windows and checks if for all the used windows */
/* a width is set - this is returned to the calling function     */
/*---------------------------------------------------------------*/

static void tds540_window_check_1( bool *is_start, bool *is_width )
{
	WINDOW *w, *wn;


	*is_start = *is_width = SET;

	for ( w = tds540.w; w != NULL; )
	{
		if ( ! w->is_used )
		{
			if ( w == tds540.w )
				wn = tds540.w = w->next;
			else
				wn = w->prev->next = w->next;

			T_free( w );
			tds540.num_windows--;
			w = wn;
			continue;
		}

		if ( ! w->is_start )
			*is_start = UNSET;
		if ( ! w->is_width )
			*is_width = UNSET;

		w = w->next;
	}
}


/*---------------------------------------------------------------------*/
/* It's not possible to set arbitrary cursor positions and distances - */
/* they've got to be multiples of the smallest time resolution of the  */
/* digitizer, which is the time base divided by TDS_POINTS_PER_DIV.    */
/* Rhe function tests if the requested cursor position and distance    */
/* fit this requirement and if not the values are readjusted. While    */
/* settings for the position and width of the window not being exact   */
/* multiples of the resultion are probably no serious errors a window  */
/* width of less than the resolution is a hint for a real problem. And */
/* while we're at it we also try to find out if all window widths are  */
/* equal - than we can use tracking cursors.                           */
/*---------------------------------------------------------------------*/

static void tds540_window_check_2( void )
{
	WINDOW *w;
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	tds540.is_equal_width = SET;

	for ( w = tds540.w; w != NULL; w = w->next )
	{
		dcs = w->start;
		dtb = tds540.timebase;
		fac = 1.0;

		while ( ( fabs( dcs ) != 0.0 && fabs( dcs ) < 1.0 ) ||
				fabs( dtb ) < 1.0 )
		{
			dcs *= 1000.0;
			dtb *= 1000.0;
			fac *= 0.001;
		}
		cs = lround( TDS_POINTS_PER_DIV * dcs );
		tb = lround( dtb );

		if ( cs % tb )        /* window start not multiple of a point ? */
		{
			cs = ( cs / tb ) * tb;
			dcs = cs * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds540_ptime( dcs ) );
			eprint( WARN, UNSET, "%s: Start point of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds540_ptime( w->start ), buffer );
			T_free( buffer );
			w->start = dcs;
		}

		dcd = w->width;
		dtb = tds540.timebase;
		fac = 1.0;

		while ( fabs( dcd ) < 1.0 || fabs( dtb ) < 1.0 )
		{
			dcd *= 1000.0;
			dtb *= 1000.0;
			fac *= 0.001;
		}
		cd = lround( TDS_POINTS_PER_DIV * dcd );
		tb = lround( dtb );

		if ( labs( cd ) < tb )     /* window smaller than one point ? */
		{
			dcd = tds540.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds540_ptime( dcd ) );
			eprint( SEVERE, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds540_ptime( w->width  ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds540_ptime( dcd ) );
			eprint( WARN, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds540_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}

		/* Check if the windows have all the same length */

		if ( w != tds540.w && w->width != tds540.w->width )
			tds540.is_equal_width = UNSET;
	}

}


/*-------------------------------------------------------------*/
/* This function checks if the windows fit into the digitizers */
/* measurement window and calculate the positions of the start */
/* and the end of the windows in units of points.              */
/*-------------------------------------------------------------*/

static void tds540_window_check_3( void )
{
	WINDOW *w;
    double window;


    window = tds540.timebase * tds540.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds540.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds540.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds540.trig_pos ) * window ||
             w->start < - tds540.trig_pos * window ||
             w->start + w->width < - tds540.trig_pos * window )
        {
			eprint( FATAL, UNSET, "%s: Window %ld doesn't fit into current "
					"digitizer time range.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}

		/* Take care: Numbers start from 1 ! */

		w->start_num = lround( ( w->start + tds540.trig_pos * window )
							   * TDS_POINTS_PER_DIV / tds540.timebase ) + 1;
		w->end_num = lround( ( w->start + w->width
							   + tds540.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds540.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			eprint( FATAL, UNSET, "%s: Window %ld has width of less than 1 "
					"point.\n",	DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}
    }
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds540_set_meas_window( WINDOW *w )
{
	tds540_set_window( w );

	if ( w != NULL )
	{
		/* If not already in gated measurement state set it now */

		if ( ! tds540.gated_state )
		{
			tds540_set_gated_meas( SET );
			tds540.gated_state = SET;
		}
	}
	else
	{
		/* If in gated measurement state switch it off */

		if ( tds540.gated_state )
		{
			tds540_set_gated_meas( UNSET );
			tds540.gated_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds540_set_curve_window( WINDOW *w )
{
	tds540_set_window( w );

	if ( w != NULL )
	{
		if ( ! tds540.snap_state )
		{
			tds540_set_snap( SET );
			tds540.snap_state = SET;
		}
	}
	else
	{
		if ( tds540.snap_state )
		{
			tds540_set_snap( UNSET );
			tds540.snap_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds540_set_window( WINDOW *w )
{
	if ( w == NULL )
		return;

	/* If all windows have the same width we only have to set the first
	   cursor (and only if its not already at the correct position),
	   otherwise we have to set both cursors */

	if ( tds540.is_equal_width )
	{
		if ( tds540.cursor_pos != w->start )
		{
			tds540_set_cursor( 1, w->start );
			tds540.cursor_pos = w->start;
		}
	}
	else
	{
		tds540_set_cursor( 1, w->start );
		tds540_set_cursor( 2, w->start + w->width );
	}
}
