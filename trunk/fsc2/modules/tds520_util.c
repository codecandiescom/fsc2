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

#include "tds520.h"

static void tds520_window_check_1( bool *is_start, bool *is_width );
static void tds520_window_check_2( void );
static void tds520_window_check_3( void );


/*-----------------------------------------------------------*/
/* Returns a string with a time value with a resonable unit. */
/*-----------------------------------------------------------*/

const char *tds520_ptime( double p_time )
{
	static char buffer[ 128 ];


	if ( fabs( p_time ) >= 1.0 )
		sprintf( buffer, "%g s", p_time );
	else if ( fabs( p_time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * p_time );
	else if ( fabs( p_time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * p_time );
	else
		sprintf( buffer, "%g ns", 1.e9 * p_time );

	return buffer;
}


/*-----------------------------------------------------------------*/
/* Deletes a window by removing it from the linked list of windows */
/*-----------------------------------------------------------------*/

void tds520_delete_windows( void )
{
	WINDOW *w;


	while ( tds520.w != NULL )
	{
		w = tds520.w;
		tds520.w = w->next;
		T_free( w );
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520_do_pre_exp_checks( void )
{
	WINDOW *w;
	bool is_start, is_width;
    double width;
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds520.is_trigger_channel )
		tds520_set_trigger_channel( Channel_Names[ tds520.trigger_channel ] );

	/* Switch on all channels that are used in the measurements */

	for ( i = 0; i <= TDS520_REF4; i++)
		if ( tds520.channels_in_use[ i ] )
			tds520_display_channel( i );

	/* Remove all unused windows and test if for all other windows the
	   start position and width is set */

	tds520_window_check_1( &is_start, &is_width);

	/* If there are no windows we're already done */

	if ( tds520.w == NULL )
		return;

	/* If start position isn't set for all windows set it to the position of
	   the left cursor */

	if ( ! is_start )
		for ( w = tds520.w; w != NULL; w = w->next )
			if ( ! w->is_start )
			{
				w->start = tds520.cursor_pos;
				w->is_start = SET;
			}

	/* If no width is set for all windows get the distance of the cursors on
	   the digitizers screen and use it as the default width. */

	if ( ! is_width )
	{
		tds520_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			eprint( FATAL, UNSET, "%s: Can't determine a reasonable value for "
					"still undefined window widths.\n", DEVICE_NAME );
			THROW( EXCEPTION )
		}

		for ( w = tds520.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursors can be positioned exactly
	   and are still within the range of the digitizers record length */

	tds520_window_check_2( );
	tds520_window_check_3( );
}


/*---------------------------------------------------------------*/
/* Removes unused windows and checks if for all the used windows */
/* a width is set - this is returned to the calling function     */
/*---------------------------------------------------------------*/

static void tds520_window_check_1( bool *is_start, bool *is_width )
{
	WINDOW *w, *wn;


	*is_start = *is_width = SET;

	for ( w = tds520.w; w != NULL; )
	{
		if ( ! w->is_used )
		{
			if ( w == tds520.w )
				wn = tds520.w = w->next;
			else
				wn = w->prev->next = w->next;

			T_free( w );
			tds520.num_windows--;
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

static void tds520_window_check_2( void )
{
	WINDOW *w;
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	for ( w = tds520.w; w != NULL; w = w->next )
	{
		dcs = w->start;
		dtb = tds520.timebase;
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
			buffer = T_strdup( tds520_ptime( dcs ) );
			eprint( WARN, UNSET, "%s: Start point of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num, 
					tds520_ptime( w->start ), buffer );
			T_free( buffer );
			w->start = dcs;
		}

		dcd = w->width;
		dtb = tds520.timebase;
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
			dcd = tds520.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520_ptime( dcd ) );
			eprint( SEVERE, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds520_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520_ptime( dcd ) );
			eprint( WARN, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds520_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
	}
}


/*-------------------------------------------------------------*/
/* This function checks if the windows fit into the digitizers */
/* measurement window and calculate the positions of the start */
/* and the end of the windows in units of points.              */
/*-------------------------------------------------------------*/

static void tds520_window_check_3( void )
{
	WINDOW *w;
    double window;


    window = tds520.timebase * tds520.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds520.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds520.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds520.trig_pos ) * window ||
             w->start < - tds520.trig_pos * window ||
             w->start + w->width < - tds520.trig_pos * window )
        {
			eprint( FATAL, UNSET, "%s: Window %ld doesn't fit into current "
					"digitizer time range.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION )
		}

		/* Take care: Numbers start from 1 ! */

		w->start_num = lrnd( ( w->start + tds520.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds520.timebase ) + 1;
		w->end_num = lrnd( ( w->start + w->width + tds520.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds520.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			eprint( FATAL, UNSET, "%s: Window %ld has width of less than 1 "
					"point.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION )
		}
    }
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

long tds520_translate_channel( int dir, long channel )
{
	if ( dir == GENERAL_TO_TDS520 )
	{
		switch ( channel )
		{
			case DIGITIZER_CHANNEL_CH1 :
				return TDS520_CH1;

			case DIGITIZER_CHANNEL_CH2 :
				return TDS520_CH2;

			case DIGITIZER_CHANNEL_MATH1 :
				return TDS520_MATH1;

			case DIGITIZER_CHANNEL_MATH2 :
				return TDS520_MATH2;

			case DIGITIZER_CHANNEL_MATH3 :
				return TDS520_MATH3;

			case DIGITIZER_CHANNEL_REF1 :
				return TDS520_REF1;

			case DIGITIZER_CHANNEL_REF2 :
				return TDS520_REF2;

			case DIGITIZER_CHANNEL_REF3 :
				return TDS520_REF3;

			case DIGITIZER_CHANNEL_REF4 :
				return TDS520_REF4;

			case DIGITIZER_CHANNEL_AUX1 :
				return TDS520_AUX1;

			case DIGITIZER_CHANNEL_AUX2 :
				return TDS520_AUX2;

			case DIGITIZER_CHANNEL_LINE :
				return TDS520_LIN;

			case DIGITIZER_CHANNEL_CH3    :
			case DIGITIZER_CHANNEL_CH4    :
			case DIGITIZER_CHANNEL_AUX    :
			case DIGITIZER_CHANNEL_MEM_C  :
			case DIGITIZER_CHANNEL_MEM_D  :
			case DIGITIZER_CHANNEL_FUNC_E :
			case DIGITIZER_CHANNEL_FUNC_F :
			case DIGITIZER_CHANNEL_EXT    :
			case DIGITIZER_CHANNEL_EXT10  :
				eprint( FATAL, SET, "%s: Digitizer has no channel %s as used "
						"in %s().\n", DEVICE_NAME,
						Digitizer_Channel_Names[ channel ], Cur_Func );
				THROW( EXCEPTION )

			default :
				eprint( FATAL, SET, "%s: Invalid channel number %ld used in "
						"%s().\n", DEVICE_NAME, channel, Cur_Func );
				THROW( EXCEPTION )
		}
	}
	else
	{
		switch ( channel )
		{
			case TDS520_CH1 :
				return DIGITIZER_CHANNEL_CH1;

			case TDS520_CH2 :
				return DIGITIZER_CHANNEL_CH2;

			case TDS520_MATH1 :
				return DIGITIZER_CHANNEL_MATH1;

			case TDS520_MATH2 :
				return DIGITIZER_CHANNEL_MATH2;

			case TDS520_MATH3 :
				return DIGITIZER_CHANNEL_MATH3;

			case TDS520_REF1 :
				return DIGITIZER_CHANNEL_REF1;

			case TDS520_REF2 :
				return DIGITIZER_CHANNEL_REF2;

			case TDS520_REF3 :
				return DIGITIZER_CHANNEL_REF3;

			case TDS520_REF4 :
				return DIGITIZER_CHANNEL_REF4;

			case TDS520_AUX1 :
				return DIGITIZER_CHANNEL_AUX1;

			case TDS520_AUX2 :
				return DIGITIZER_CHANNEL_AUX2;

			case TDS520_LIN :
				return DIGITIZER_CHANNEL_LINE;

			default :
				eprint( FATAL, UNSET, "Internal error detected at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION )
		}
	}

	fsc2_assert( 1 == 0 );
	return -1;
}
