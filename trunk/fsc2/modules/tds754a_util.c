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


#include "tds754a.h"


static void tds754a_window_check_1( bool *is_start, bool *is_width );
static void tds754a_window_check_2( WINDOW *w );
static void tds754a_window_check_3( WINDOW *w );


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

const char *tds754a_ptime( double p_time )
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

void tds754a_delete_windows( TDS754A *s )
{
	WINDOW *w;

	while ( s->w != NULL )
	{
		w = s->w;
		s->w = w->next;
		T_free( w );
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds754a_do_pre_exp_checks( void )
{
	WINDOW *w;
	bool is_start, is_width;
    double width;


	/* Remove all unused windows and test if for all other windows the start
	   position and the width is set */

	tds754a_window_check_1( &is_start, &is_width );

	/* That's all if no windows have been defined we switch off gated
	   measurement mode, i.e. all measurement operations are automatically
	   done on the whole curve */

	if ( tds754a.w == NULL )
	{
		tds754a_set_gated_meas( UNSET );
		tds754a.gated_state = UNSET;
		return;
	}

	/* If start position hasn't been set for all windows set it to the
	   position of the left cursor on the digitizer screen */

	if ( ! is_start )
		for ( w = tds754a.w; w != NULL; w = w->next )
			if ( ! w->is_start )
			{
				w->start = tds754a.cursor_pos;
				w->is_start = SET;
			}

	/* If witdh hasn't been set for all windows get the distance of the
	   cursors on the digitizer screen and use it as the default width. */

	if ( ! is_width )
	{
		width = tds754a_get_cursor_distance( );

		width = fabs( width );

		if ( width == 0.0 )
		{
			print( FATAL, "Can't determine a reasonable value for the missing "
				   "window widths.\n" );
			THROW( EXCEPTION );
		}

		for ( w = tds754a.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursors can be positioned exactly
	   and are still within the range of the digitizers record length */

	for ( w = tds754a.w; w != NULL; w = w->next )
		tds754a_window_checks( w );

	/* Now that all windows are properly set we switch on gated measurements */

	tds754a_set_gated_meas( SET );
	tds754a.gated_state = SET;

	/* If the widths of all windows are equal we switch on tracking cursor
	   mode and set the cursors to the position of the first window */

	tds754a_set_tracking( tds754a.w );
}


/*------------------------------------------*/
/* Checks if for all windows a width is set */
/*------------------------------------------*/

static void tds754a_window_check_1( bool *is_start, bool *is_width )
{
	WINDOW *w;


	*is_start = *is_width = SET;

	for ( w = tds754a.w; w != NULL; w = w->next )
	{
		if ( ! w->is_start )
			*is_start = UNSET;
		if ( ! w->is_width )
			*is_width = UNSET;
	}
}


/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

void tds754a_window_checks( WINDOW *w )
{
	tds754a_window_check_2( w );
	tds754a_window_check_3( w );
}


/*---------------------------------------------------------------------*/
/* It's not possible to set arbitrary cursor positions and distances,  */
/* they have to be multiples of the smallest time resolution of the    */
/* digitizer which is the time base divided by TDS754A_POINTS_PER_DIV. */
/* The function tests if the requested cursor positions and distance   */
/* fit these requirements and if not the values are readjusted. While  */
/* settings for the position and width of the window not being exact   */
/* multiples of the resultion are probably no serious errors a window  */
/* width of less than the resolution is a hint for a real problem. And */
/* while we're at it we also try to find out if all window widths are  */
/* equal - than we can use tracking cursors.                           */
/*---------------------------------------------------------------------*/

static void tds754a_window_check_2( WINDOW *w )
{
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	/* Can't do checks if window position or digitizers timebase is unknown */

	if ( FSC2_MODE != EXPERIMENT &&
		 ( ! w->is_start || ! tds754a.is_timebase ) )
		return;

	dcs = w->start;
	dtb = tds754a.timebase;
	fac = 1.0;

	while ( ( fabs( dcs ) != 0.0 && fabs( dcs ) < 1.0 ) ||
			fabs( dtb ) < 1.0 )
	{
		dcs *= 1000.0;
		dtb *= 1000.0;
		fac *= 0.001;
	}

	cs = lrnd( TDS754A_POINTS_PER_DIV * dcs );
	tb = lrnd( dtb );

	if ( cs % tb )        /* window start not multiple of a point ? */
	{
		cs = ( cs / tb ) * tb;
		dcs = cs * fac / TDS754A_POINTS_PER_DIV;
		buffer = T_strdup( tds754a_ptime( dcs ) );
		print( WARN, "Start point of %ld. window had to be readjusted from "
			   "%s to %s.\n", w->num + 1 - WINDOW_START_NUMBER,
			   tds754a_ptime( w->start ), buffer );
		T_free( buffer );
		w->start = dcs;
	}

	/* Can't do further checks if window width is still unknown */

	if ( ! w->is_width )
		return;

	dcd = w->width;
	dtb = tds754a.timebase;
	fac = 1.0;

	while ( fabs( dcd ) < 1.0 || fabs( dtb ) < 1.0 )
	{
		dcd *= 1000.0;
		dtb *= 1000.0;
		fac *= 0.001;
	}

	cd = lrnd( TDS754A_POINTS_PER_DIV * dcd );
	tb = lrnd( dtb );

	if ( labs( cd ) < tb )         /* window smaller than one point ? */
	{
		dcd = tds754a.timebase / TDS754A_POINTS_PER_DIV;
		buffer = T_strdup( tds754a_ptime( dcd ) );
		print( SEVERE, "Width of %ld. window had to be readjusted from %s to "
			   "%s.\n", w->num + 1 - WINDOW_START_NUMBER,
			   tds754a_ptime( w->width ), buffer );
		T_free( buffer );
		w->width = dcd;
	}
	else if ( cd % tb )            /* window width not multiple of a point ? */
	{
		cd = ( cd / tb ) * tb;
		dcd = cd * fac / TDS754A_POINTS_PER_DIV;
		buffer = T_strdup( tds754a_ptime( dcd ) );
		print( WARN, "Width of %ld. window had to be readjusted from %s to "
			   "%s.\n", w->num + 1 - WINDOW_START_NUMBER,
			   tds754a_ptime( w->width ), buffer );
		T_free( buffer );
		w->width = dcd;
	}
}


/*-------------------------------------------------------------*/
/* This function checks if the window fits into the digitizers */
/* measurement window and calculates the position of the start */
/* and the end of the windows in units of points.              */
/*-------------------------------------------------------------*/

static void tds754a_window_check_3( WINDOW *w )
{
    double window;
	char *buf1, *buf2, *buf3;
	double start, width;


	/* Can't do checks if timebase, record length, trigger position,
	   window position or width is still unknown */

	if ( FSC2_MODE != EXPERIMENT && 
		 ( ! tds754a.is_timebase || ! tds754a.is_rec_len ||
		   ! tds754a.is_trig_pos || ! w->is_start || ! w->is_width ) )
		return;

	/* Calculate the start and end position of the window in digitizer
	   point units (take care: point numbers start with 1 and the end number
	   is *included* when fetching a curve) */

    window = tds754a.timebase * tds754a.rec_len / TDS754A_POINTS_PER_DIV;

	w->start_num = lrnd( ( w->start + tds754a.trig_pos * window )
						 * TDS754A_POINTS_PER_DIV / tds754a.timebase ) + 1;
	w->end_num = lrnd( ( w->start + w->width + tds754a.trig_pos * window )
						 * TDS754A_POINTS_PER_DIV / tds754a.timebase );

	fsc2_assert( w->start_num <= w->end_num );

	/* Check that window does not start too early */

	if ( w->start_num < 1 )
	{
		if ( FSC2_MODE != EXPERIMENT )
		{
			print( FATAL, " %ld. window starts too early.\n",
				   w->num + 1 - WINDOW_START_NUMBER );
			THROW( EXCEPTION );
		}

		/* Move the start position of the window to the first point */

		w->start_num = 1;
		start = - tds754a.trig_pos * window;

		/* If the end point is still within the record length nothing more
		   got to be done, otheriwse the window width has to be reduced */

		if ( w->end_num - w->start_num + 1 <= tds754a.rec_len )
		{
			buf1 = T_strdup( tds754a_ptime( w->start ) );
			print( SEVERE, "Position of %ld. window had to be shifted from "
				   "%s to %s.\n", w->num + 1 - WINDOW_START_NUMBER, buf1,
				   tds754a_ptime( start ) );
			T_free( buf1 );

			w->end_num = w->end_num - w->start_num + 1;
			w->start = start;
		}
		else
		{
			w->end_num = tds754a.rec_len;
			width = w->end_num * tds754a.timebase / TDS754A_POINTS_PER_DIV;

			buf1 = T_strdup( tds754a_ptime( w->start ) );
			buf2 = T_strdup( tds754a_ptime( w->width ) );
			buf3 = T_strdup( tds754a_ptime( start ) );

			print( SEVERE, "Position and width of %ld. window had to be "
				   "readjusted from %s and %s to %s and %s.\n",
				   w->num + 1 - WINDOW_START_NUMBER, buf1, buf2, buf3,
				   tds754a_ptime( width ) );

			T_free( buf3 );
			T_free( buf2 );
			T_free( buf1 );

			w->start = start;
			w->width = width;
		}
	}

	/* If the start position of the window is in the correct range but
	   the end point is too late reduce the window width (at least if we're
	   already running the experiment */

	if ( w->start_num < tds754a.rec_len && w->end_num > tds754a.rec_len )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( FATAL, "Width of %ld. window is too large.\n",
				   w->num + 1 - WINDOW_START_NUMBER );
			THROW( EXCEPTION );
		}

		w->end_num = tds754a.rec_len;
		width = ( tds754a.rec_len - w->start_num + 1 )
				* tds754a.timebase / TDS754A_POINTS_PER_DIV;

		buf1 = T_strdup( tds754a_ptime( w->width ) );
		print( SEVERE, "Width of %ld. window had to be readjusted from %s to "
			   "%s.\n", w->num + 1 - WINDOW_START_NUMBER, buf1,
			   tds754a_ptime( width ) );
		T_free( buf1 );

		w->width = width;
	}

	/* Check that the window doesn't start too late */

	if ( w->start_num > tds754a.rec_len )
	{
		if ( FSC2_MODE == EXPERIMENT )
		{
			print( FATAL, "%ld. window starts too late.\n",
				   w->num + 1 - WINDOW_START_NUMBER );
			THROW( EXCEPTION );
		}

		/* If the window width isn't larger than the record length shift
		   the window start position to the latest possible point, otherwise
		   use the whole record length */

		if ( w->end_num - w->start_num + 1 <= tds754a.rec_len )
		{
			w->start_num = tds754a.rec_len - ( w->end_num - w->start_num );
			start = ( w->start_num - 1 )
					* tds754a.timebase / TDS754A_POINTS_PER_DIV
					- tds754a.trig_pos * window;
			w->end_num = tds754a.rec_len;

			buf1 = T_strdup( tds754a_ptime( w->start ) );
			print( SEVERE, "Position of %ld. window had to be shifted from "
				   "%s to %s.\n", w->num + 1 - WINDOW_START_NUMBER, buf1,
				   tds754a_ptime( start ) );
			T_free( buf1 );

			w->start = start;
		}
		else
		{
			w->start_num = 1;
			start = - tds754a.trig_pos * window;
			w->end_num = tds754a.rec_len;
			width = tds754a.rec_len 
					* tds754a.timebase / TDS754A_POINTS_PER_DIV;

			buf1 = T_strdup( tds754a_ptime( w->start ) );
			buf2 = T_strdup( tds754a_ptime( w->width ) );
			buf3 = T_strdup( tds754a_ptime( start ) );

			print( SEVERE, "Position and width of %ld. window had to be "
				   "readjusted from %s and %s to %s and %s.\n",
				   w->num + 1 - WINDOW_START_NUMBER, buf1, buf2, buf3,
				   tds754a_ptime( width ) );

			T_free( buf3 );
			T_free( buf2 );
			T_free( buf1 );

			w->start = start;
			w->width = width;
		}
	}
}


/*-------------------------------------------------------------------*/
/* This function fisrt checks if all window widths are idendentical. */
/* Then it positions the cursors at the end points of the window the */
/* function got passed (or to both ends of the digitizers record     */
/* length if a NULL pointer was passed to the function. Finally, it  */
/* switches tracking cursors on or off, depending if the widths of   */
/* all windows are equal or not.                                     */
/*-------------------------------------------------------------------*/

void tds754a_set_tracking( WINDOW *w )
{
	WINDOW *ww;
	double window;


	/* Check if we can use tracking cursors, i.e. if all window widths are
	   equal */

	tds754a.is_equal_width = SET;

	if ( tds754a.w != NULL )
		for ( ww = tds754a.w->next; ww != NULL; ww = ww->next )
			if ( tds754a.w->width != ww->width )
			{
				tds754a.is_equal_width = UNSET;
				break;
			}

	/* Set the cursor to the positions of the window we got passed as
	   argument (switch off tracking before we do so) and then switch
	   tracking cursors on if the window widths are all equal. */

    window = tds754a.timebase * tds754a.rec_len / TDS754A_POINTS_PER_DIV;

	if ( FSC2_MODE == EXPERIMENT )
	{
		tds754a_set_track_cursors( UNSET );
		if ( w != NULL )
		{
			tds754a_set_cursor( 1, w->start );
			tds754a_set_cursor( 2, w->start + w->width );
		}
		else
		{
			tds754a_set_cursor( 1, - tds754a.trig_pos * window );
			tds754a_set_cursor( 2, ( 1.0 - tds754a.trig_pos ) * window );
		}
	}

	tds754a.cursor_pos = w != NULL ? w->start : - tds754a.trig_pos * window;

	if ( FSC2_MODE == EXPERIMENT )
		tds754a_set_track_cursors( tds754a.is_equal_width );
}
	

/*------------------------------------------------------------------*/
/* This function is called for measurements on curves (i.e. area or */
/* amplitude). If a window gets passed to the function the cursors  */
/* positioned a the ends of the window and gated measurement mode   */
/* is switched on (i.e. the device will do the calculation on the   */
/* points between the cursors automatically), otherwise gated       */
/* measurement mode is switched off and the device will do the      */
/* calculation on the complete curve.                               */
/*------------------------------------------------------------------*/

void tds754a_set_meas_window( WINDOW *w )
{
	tds754a_set_window( w );

	if ( w != NULL )
	{
		/* If not already in gated measurement state set it now */

		if ( ! tds754a.gated_state )
		{
			tds754a_set_gated_meas( SET );
			tds754a.gated_state = SET;
		}
	}
	else
	{
		/* If in gated measurement state switch it off */

		if ( tds754a.gated_state )
		{
			tds754a_set_gated_meas( UNSET );
			tds754a.gated_state = UNSET;
		}
	}
}


/*----------------------------------------------------------*/
/* This function is the equivalent to the previous function */
/* 'tds754a_set_meas_window()' but for cases were not a     */
/* measurement on a curve is to be done but when a curve    */
/* is to be fetched from the digitizer.                     */
/*----------------------------------------------------------*/

void tds754a_set_curve_window( WINDOW *w )
{
	tds754a_set_window( w );

	if ( w != NULL )
	{
		if ( ! tds754a.snap_state )
		{
			tds754a_set_snap( SET );
			tds754a.snap_state = SET;
		}
	}
	else
	{
		if ( tds754a.snap_state )
		{
			tds754a_set_snap( UNSET );
			tds754a.snap_state = UNSET;
		}
	}
}


/*----------------------------------------------------------------*/
/* This function sets the cursors to the end points of the window */
/* it got passed as its argument. If all windows have equal width */
/* we can be sure tracking mode has already been set and only the */
/* first cursor must be repositioned, otherwise both cursors need */
/* to be set.                                                     */
/*----------------------------------------------------------------*/

void tds754a_set_window( WINDOW *w )
{
	if ( w == NULL )
		return;

	/* If all windows have the same width we only have to set the first
	   cursor (and only if its not already at the correct position),
	   otherwise we have to set both cursors */

	if ( tds754a.is_equal_width )
	{
		if ( tds754a.cursor_pos != w->start )
		{
			tds754a_set_cursor( 1, w->start );
			tds754a.cursor_pos = w->start;
		}
	}
	else
	{
		tds754a_set_cursor( 1, w->start );
		tds754a_set_cursor( 2, w->start + w->width );
	}

	tds754a.cursor_pos = w->start;
}


/*--------------------------------------------------------------*/
/* The function is used to translate back and forth between the */
/* channel numbers the way the user specifies them in the EDL   */
/* program and the channel numbers as specified in the header   */
/* file.                                                        */
/*--------------------------------------------------------------*/

long tds754a_translate_channel( int dir, long channel )
{
	if ( dir == GENERAL_TO_TDS754A )
	{
		switch ( channel )
		{
			case DIGITIZER_CHANNEL_CH1 :
				return TDS754A_CH1;

			case DIGITIZER_CHANNEL_CH2 :
				return TDS754A_CH2;

			case DIGITIZER_CHANNEL_CH3 :
				return TDS754A_CH3;

			case DIGITIZER_CHANNEL_CH4 :
				return TDS754A_CH4;

			case DIGITIZER_CHANNEL_MATH1 :
				return TDS754A_MATH1;

			case DIGITIZER_CHANNEL_MATH2 :
				return TDS754A_MATH2;

			case DIGITIZER_CHANNEL_MATH3 :
				return TDS754A_MATH3;

			case DIGITIZER_CHANNEL_REF1 :
				return TDS754A_REF1;

			case DIGITIZER_CHANNEL_REF2 :
				return TDS754A_REF2;

			case DIGITIZER_CHANNEL_REF3 :
				return TDS754A_REF3;

			case DIGITIZER_CHANNEL_REF4 :
				return TDS754A_REF4;

			case DIGITIZER_CHANNEL_AUX :
				return TDS754A_AUX;

			case DIGITIZER_CHANNEL_LINE :
				return TDS754A_LIN;

			case DIGITIZER_CHANNEL_AUX1   :
			case DIGITIZER_CHANNEL_AUX2   :
			case DIGITIZER_CHANNEL_MEM_C  :
			case DIGITIZER_CHANNEL_MEM_D  :
			case DIGITIZER_CHANNEL_FUNC_E :
			case DIGITIZER_CHANNEL_FUNC_F :
			case DIGITIZER_CHANNEL_EXT    :
			case DIGITIZER_CHANNEL_EXT10  :
				print( FATAL, "Digitizer has no channel %s.\n",
					   Digitizer_Channel_Names[ channel ] );
				THROW( EXCEPTION );

			default :
				print( FATAL, "Invalid channel number %ld.\n", channel );
				THROW( EXCEPTION );
		}
	}
	else
	{
		switch ( channel )
		{
			case TDS754A_CH1 :
				return DIGITIZER_CHANNEL_CH1;

			case TDS754A_CH2 :
				return DIGITIZER_CHANNEL_CH2;

			case TDS754A_CH3 :
				return DIGITIZER_CHANNEL_CH3;

			case TDS754A_CH4 :
				return DIGITIZER_CHANNEL_CH4;

			case TDS754A_MATH1 :
				return DIGITIZER_CHANNEL_MATH1;

			case TDS754A_MATH2 :
				return DIGITIZER_CHANNEL_MATH2;

			case TDS754A_MATH3 :
				return DIGITIZER_CHANNEL_MATH3;

			case TDS754A_REF1 :
				return DIGITIZER_CHANNEL_REF1;

			case TDS754A_REF2 :
				return DIGITIZER_CHANNEL_REF2;

			case TDS754A_REF3 :
				return DIGITIZER_CHANNEL_REF3;

			case TDS754A_REF4 :
				return DIGITIZER_CHANNEL_REF4;

			case TDS754A_AUX :
				return DIGITIZER_CHANNEL_AUX;

			case TDS754A_LIN :
				return DIGITIZER_CHANNEL_LINE;

			default :
				print( FATAL, "Internal error detected at %s:%d.\n",
					   __FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	fsc2_assert( 1 == 0 );
	return -1;
}


/*-----------------------------------------------------------------*/
/* During preparations section several function calls change the   */
/* structure describing the digitzers internal state. When the     */
/* experiment starts the digitizer has to be set up to coincide    */
/* with this state. But during the test run as well as during      */
/* the experiment this state description usually becomes changed.  */
/* To be able to repeat the experiment always starting with the    */
/* same state of the digitizer (at least as far as the user has    */
/* defined it, the state directly before the test run or an        */
/* experiment has to be stored and, following a test run or an     */
/* experiment has to be reset. And that's what this function does. */
/*-----------------------------------------------------------------*/

void tds754a_store_state( TDS754A *dest, TDS754A *src )
{
	WINDOW *w, *dw;


	while ( dest->w != NULL )
	{
		w = dest->w;
		dest->w = w->next;
		T_free( w );
	}

	memcpy( dest, src, sizeof( TDS754A ) );

	if ( src->num_windows == 0 )
	{
		dest->w = NULL;
		return;
	}

	dw = dest->w = T_malloc( sizeof( WINDOW ) );
	memcpy( dest->w, src->w, sizeof( WINDOW ) );
	dest->w->next = dest->w->prev = NULL;

	for ( w = src->w->next; w != NULL; w = w->next )
	{
		dw->next = T_malloc( sizeof( WINDOW ) );
		memcpy( dw->next, w, sizeof( WINDOW ) );
		dw->next->prev = dw;
		dw = dw->next;
		dw->next = NULL;
	}
}


/*--------------------------------------------------------------*/
/* Whenever the user changes timebase, record length or trigger */
/* position either via the program or manually (if the lock of  */
/* the keyboard is switched off) the positions of the windows   */
/* have to be recalculated and, if necessary, readjusted.       */
/* Unfortunately, we can't check without a lot of additional    */
/* work if the user changes on of the parameters mentioned      */
/* above manually. Therefor, this function should be called     */
/* whenever a function might change, query or just rely on the  */
/* validity of this parameters and the digitizers keyboard      */
/* isn't locked.                                                */
/*--------------------------------------------------------------*/

void tds754a_state_check( double timebase, long rec_len, double trig_pos )
{
	WINDOW *w;
	double window;
	char *buf1, *buf2, *buf3;
	double start, width;


	/* During the experiment we don't care about the arguments passed to the
	   function but ask the digitizer (and do nothing if they didn't change).
	   During the test run or preparation we can do the checks only if the
	   timebase, record length and trigger position have already been set. */

	if ( FSC2_MODE == EXPERIMENT )
	{
		timebase = tds754a_get_timebase( );
		rec_len = tds754a_get_record_length( );
		trig_pos = tds754a_get_trigger_pos( );

		if ( timebase == tds754a.timebase &&
			 rec_len  == tds754a.rec_len  &&
			 trig_pos == tds754a.trig_pos )
			return;
	}
	else
	{
		if ( ! tds754a.is_timebase || ! tds754a.is_rec_len ||
			 ! tds754a.is_trig_pos )
			return;
	}

	/* In principle, the windows should simply stay at their positions (as
	   the cursors do) but if the record length got smaller we have to do
	   also some checking and, if necessary, readjustments.*/

    window = timebase * rec_len / TDS754A_POINTS_PER_DIV;

	for ( w = tds754a.w; w != NULL; w = w->next )
	{
		if ( rec_len < tds754a.rec_len )
		{
			/* If the start of window is still within the valid range but the
			   end is too late reduce the length. */

			if ( w->start_num < rec_len && w->end_num > rec_len )
			{
				w->end_num = rec_len;
				width = ( rec_len - w->start_num + 1 )
					    * timebase / TDS754A_POINTS_PER_DIV;

				buf1 = T_strdup( tds754a_ptime( w->width ) );
				print( SEVERE, "Width of %ld. window had to be readjusted "
					   "from %s to %s.\n", w->num + 1 - WINDOW_START_NUMBER,
					   buf1, tds754a_ptime( width ) );
				T_free( buf1 );

				w->width = width;
			}
			else if ( w->start_num > rec_len )
			{
				/* If the window width isn't larger than the record length
				   shift the window start position to the latest possible
				   point, otherwise use the whole record length */

				if ( w->end_num - w->start_num + 1 <= rec_len )
				{
					w->start_num = rec_len - ( w->end_num - w->start_num );
					start = ( w->start_num - 1 )
							* timebase / TDS754A_POINTS_PER_DIV
							- trig_pos * window;
					w->end_num = rec_len;

					buf1 = T_strdup( tds754a_ptime( w->start ) );
					print( SEVERE, "Position of %ld. window had to be shifted "
						   "from %s to %s.\n",
						   w->num + 1 - WINDOW_START_NUMBER, buf1,
						   tds754a_ptime( start ) );
					T_free( buf1 );
				}
				else
				{
					w->start_num = 1;
					start = - trig_pos * window;
					w->end_num = rec_len;
					width = rec_len * timebase / TDS754A_POINTS_PER_DIV;

					buf1 = T_strdup( tds754a_ptime( w->start ) );
					buf2 = T_strdup( tds754a_ptime( w->width ) );
					buf3 = T_strdup( tds754a_ptime( start ) );

					print( SEVERE, "Position and width of %ld. window had to "
						   "be readjusted from %s and %s to %s and %s.\n",
						   w->num + 1 - WINDOW_START_NUMBER, buf1, buf2, buf3,
						   tds754a_ptime( width ) );

					T_free( buf3 );
					T_free( buf2 );
					T_free( buf1 );
				}
			}
		}

		/* Recalculate the windows start time and width (we assume it just
		   stays in exacty the same position) */

		w->start = ( w->start_num - 1 ) * timebase / TDS754A_POINTS_PER_DIV
				   - trig_pos * window;
		w->width = ( w->end_num - 1 ) * timebase / TDS754A_POINTS_PER_DIV
				   - trig_pos * window - w->start;
	}

	tds754a.timebase = timebase;
	tds754a.rec_len  = rec_len;
	tds754a.trig_pos = trig_pos;

	if ( FSC2_MODE == EXPERIMENT )
		tds754a.cursor_pos = tds754a_get_cursor_position( 1 );
}	


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
