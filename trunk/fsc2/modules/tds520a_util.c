/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

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


#include "tds520a.h"

static void tds520a_window_check_1( bool *is_start, bool *is_width );
static void tds520a_window_check_2( void );
static void tds520a_window_check_3( void );


/*-----------------------------------------------------------*/
/* Returns a string with a time value with a resonable unit. */
/*-----------------------------------------------------------*/

const char *tds520a_ptime( double time )
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

void tds520a_delete_windows( void )
{
	WINDOW *w;

	while ( tds520a.w != NULL )
	{
		w = tds520a.w;
		tds520a.w = w->next;
		T_free( w );
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520a_do_pre_exp_checks( void )
{
	WINDOW *w;
	bool is_start, is_width;
    double width;
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds520a.is_trigger_channel )
		tds520a_set_trigger_channel( 
			Channel_Names[ tds520a.trigger_channel ] );

	/* Switch all channels on that get used in the measurements */

	for ( i = 0; i <= TDS520A_REF4; i++)
		if ( tds520a.channels_in_use[ i ] )
			tds520a_display_channel( i );

	/* Remove all unused windows and test if for all other windows the start
	   position and the width is set */

	tds520a_window_check_1( &is_start, &is_width);

	/* That's all if no windows have been defined, we just switch off gated
	   measurement mode, i.e. all measurement operations are done on the whole
	   curve */

	if ( tds520a.w == NULL )
	{
		tds520a_set_gated_meas( UNSET );
		tds520a.gated_state = UNSET;
		return;
	}

	/* If start position isn't set for all windows set it to the position of
	   the left cursor */

	if ( ! is_start )
		for ( w = tds520a.w; w != NULL; w = w->next )
			if ( ! w->is_start )
			{
				w->start = tds520a.cursor_pos;
				w->is_start = SET;
			}

	/* If not get the distance of the cursors on the digitizers screen and
	   use it as the default width. */

	if ( ! is_width )
	{
		tds520a_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			eprint( FATAL, UNSET, "%s: Can't determine a reasonable value for "
					"still undefined window widths.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		for ( w = tds520a.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursorsd can be positioned exactly
	   and are still within the range of the digitizers record length */

	tds520a_window_check_2( );
	tds520a_window_check_3( );

	/* Now that all windows are properly set we switch on gated measurements */

	tds520a_set_gated_meas( SET );
	tds520a.gated_state = SET;

	/* If the widths of all windows are equal we switch on tracking cursor
	   mode and set the cursors to the position of the first window */

	if ( tds520a.is_equal_width )
	{
		tds520a_set_cursor( 1, tds520a.w->start );
		tds520a_set_cursor( 2, tds520a.w->start + tds520a.w->width );
		tds520a_set_track_cursors( SET );
		tds520a.cursor_pos = tds520a.w->start;
	}
	else
		tds520a_set_track_cursors( UNSET );
}


/*---------------------------------------------------------------*/
/* Removes unused windows and checks if for all the used windows */
/* a width is set - this is returned to the calling function     */
/*---------------------------------------------------------------*/

static void tds520a_window_check_1( bool *is_start, bool *is_width )
{
	WINDOW *w, *wn;


	*is_start = *is_width = SET;

	for ( w = tds520a.w; w != NULL; )
	{
		if ( ! w->is_used )
		{
			if ( w == tds520a.w )
				wn = tds520a.w = w->next;
			else
				wn = w->prev->next = w->next;

			T_free( w );
			tds520a.num_windows--;
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

static void tds520a_window_check_2( void )
{
	WINDOW *w;
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	tds520a.is_equal_width = SET;

	for ( w = tds520a.w; w != NULL; w = w->next )
	{
		dcs = w->start;
		dtb = tds520a.timebase;
		fac = 1.0;

		while ( ( fabs( dcs ) != 0.0 && fabs( dcs ) < 1.0 ) ||
				fabs( dtb ) < 1.0 )
		{
			dcs *= 1000.0;
			dtb *= 1000.0;
			fac *= 0.001;
		}
		cs = lrnd( TDS_POINTS_PER_DIV * dcs );
		tb = lrnd( dtb );

		if ( cs % tb )        /* window start not multiple of a point ? */
		{
			cs = ( cs / tb ) * tb;
			dcs = cs * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520a_ptime( dcs ) );
			eprint( WARN, UNSET, "%s: Start point of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds520a_ptime( w->start ), buffer );
			T_free( buffer );
			w->start = dcs;
		}

		dcd = w->width;
		dtb = tds520a.timebase;
		fac = 1.0;

		while ( fabs( dcd ) < 1.0 || fabs( dtb ) < 1.0 )
		{
			dcd *= 1000.0;
			dtb *= 1000.0;
			fac *= 0.001;
		}
		cd = lrnd( TDS_POINTS_PER_DIV * dcd );
		tb = lrnd( dtb );

		if ( labs( cd ) < tb )     /* window smaller than one point ? */
		{
			dcd = tds520a.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520a_ptime( dcd ) );
			eprint( SEVERE, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds520a_ptime( w->width  ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520a_ptime( dcd ) );
			eprint( WARN, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds520a_ptime( w->width  ), buffer );
			T_free( buffer );
			w->width = dcd;
		}

		/* Check if the windows have all the same length */

		if ( w != tds520a.w && w->width != tds520a.w->width )
			tds520a.is_equal_width = UNSET;
	}
}


/*-------------------------------------------------------------*/
/* This function checks if the windows fit into the digitizers */
/* measurement window and calculate the positions of the start */
/* and the end of the windows in units of points.              */
/*-------------------------------------------------------------*/

static void tds520a_window_check_3( void )
{
	WINDOW *w;
    double window;


    window = tds520a.timebase * tds520a.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds520a.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds520a.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds520a.trig_pos ) * window ||
             w->start < - tds520a.trig_pos * window ||
             w->start + w->width < - tds520a.trig_pos * window )
        {
			eprint( FATAL, UNSET, "%s: Window %ld doesn't fit into current "
					"digitizer time range.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION )
		}

		/* Take care: Numbers start from 1 ! */

		w->start_num = lrnd( ( w->start + tds520a.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds520a.timebase ) + 1;
		w->end_num = lrnd( ( w->start + w->width
							 + tds520a.trig_pos * window )
						   * TDS_POINTS_PER_DIV / tds520a.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			eprint( FATAL, UNSET, "%s: Window %ld has width of less than 1 "
					"point.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION )
		}
    }
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520a_set_meas_window( WINDOW *w )
{
	tds520a_set_window( w );

	if ( w != NULL )
	{
		/* If not already in gated measurement state set it now */

		if ( ! tds520a.gated_state )
		{
			tds520a_set_gated_meas( SET );
			tds520a.gated_state = SET;
		}
	}
	else
	{
		/* If in gated measurement state switch it off */

		if ( tds520a.gated_state )
		{
			tds520a_set_gated_meas( UNSET );
			tds520a.gated_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520a_set_curve_window( WINDOW *w )
{
	tds520a_set_window( w );

	if ( w != NULL )
	{
		if ( ! tds520a.snap_state )
		{
			tds520a_set_snap( SET );
			tds520a.snap_state = SET;
		}
	}
	else
	{
		if ( tds520a.snap_state )
		{
			tds520a_set_snap( UNSET );
			tds520a.snap_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520a_set_window( WINDOW *w )
{
	if ( w == NULL )
		return;

	/* If all windows have the same width we only have to set the first
	   cursor (and only if its not already at the correct position),
	   otherwise we have to set both cursors */

	if ( tds520a.is_equal_width )
	{
		if ( tds520a.cursor_pos != w->start )
		{
			tds520a_set_cursor( 1, w->start );
			tds520a.cursor_pos = w->start;
		}
	}
	else
	{
		tds520a_set_cursor( 1, w->start );
		tds520a_set_cursor( 2, w->start + w->width );
	}
}
