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

#include "lecroy9400.h"

#if 0
static void lecroy9400_window_check_1( bool *is_start, bool *is_width );
static void lecroy9400_window_check_2( void );
static void lecroy9400_window_check_3( void );
#endif


/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

int lecroy9400_get_tb_index( double timebase )
{
	unsigned int i;

	for ( i = 0; i < TB_ENTRIES - 1; i++ )
		if ( timebase >= tb[ i ] && timebase <= tb[ i + 1 ] )
			return i + ( ( tb[ i ] / timebase > timebase / tb[ i + 1 ] ) ?
						 0 : 1 );

	return i;
}


/*-----------------------------------------------------------*/
/* Returns a string with a time value with a resonable unit. */
/*-----------------------------------------------------------*/

const char *lecroy9400_ptime( double p_time )
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

void lecroy9400_delete_windows( LECROY9400 *s )
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

void lecroy9400_do_pre_exp_checks( void )
{
#if 0
	WINDOW *w;
	bool is_start, is_width;
    double width;
#endif
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( lecroy9400.is_trigger_channel )
		lecroy9400_set_trigger_source( lecroy9400.trigger_channel );

	/* Switch on all channels that are used in the measurements */

	for ( lecroy9400.num_used_channels = 0, i = 0; i <= LECROY9400_FUNC_F; i++)
		lecroy9400_display( i, lecroy9400.channels_in_use[ i ] );

	/* Remove all unused windows and test if for all other windows the
	   start position and width is set */

#if 0
	lecroy9400_window_check_1( &is_start, &is_width);

	/* If there are no windows we're already done */

	if ( lecroy9400.w == NULL )
#endif
		return;

#if 0

	/* If start position isn't set for all windows set it to the position of
	   the left cursor */

	if ( ! is_start )
		for ( w = lecroy9400.w; w != NULL; w = w->next )
			if ( ! w->is_start )
			{
				w->start = lecroy9400.cursor_pos;
				w->is_start = SET;
			}

	/* If no width is set for all windows get the distance of the cursors on
	   the digitizers screen and use it as the default width. */

	if ( ! is_width )
	{
		lecroy9400_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			print( FATAL, "Can't determine a reasonable value for still "
				   "undefined window widths.\n" );
			THROW( EXCEPTION );
		}

		for ( w = lecroy9400.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursors can be positioned exactly
	   and are still within the range of the digitizers record length */

	lecroy9400_window_check_2( );
	lecroy9400_window_check_3( );

#endif
}

#if 0
/*---------------------------------------------------------------*/
/* Removes unused windows and checks if for all the used windows */
/* a width is set - this is returned to the calling function     */
/*---------------------------------------------------------------*/

static void lecroy9400_window_check_1( bool *is_start, bool *is_width )
{
	WINDOW *w;


	*is_start = *is_width = SET;

	for ( w = lecroy9400.w; w != NULL; w = w->next )
	{
		if ( ! w->is_start )
			*is_start = UNSET;

		if ( ! w->is_width )
			*is_width = UNSET;
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

static void lecroy9400_window_check_2( void )
{
	WINDOW *w;
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	for ( w = lecroy9400.w; w != NULL; w = w->next )
	{
		dcs = w->start;
		dtb = lecroy9400.timebase;
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
			buffer = T_strdup( lecroy9400_ptime( dcs ) );
			print( WARN, "Start point of window %ld had to be readjusted from "
				   "%s to %s.\n",
				   w->num, lecroy9400_ptime( w->start ), buffer );
			T_free( buffer );
			w->start = dcs;
		}

		dcd = w->width;
		dtb = lecroy9400.timebase;
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
			dcd = lecroy9400.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( lecroy9400_ptime( dcd ) );
			print( SEVERE, "Width of window %ld had to be readjusted from %s "
				   "to %s.\n", w->num, lecroy9400_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( lecroy9400_ptime( dcd ) );
			print( WARN, "Width of window %ld had to be readjusted from %s to "
				   "%s.\n", w->num, lecroy9400_ptime( w->width ), buffer );
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

static void lecroy9400_window_check_3( void )
{
	WINDOW *w;
    double window;


    window = lecroy9400.timebase * lecroy9400.rec_len / TDS_POINTS_PER_DIV;

    for ( w = lecroy9400.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - lecroy9400.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - lecroy9400.trig_pos ) * window ||
             w->start < - lecroy9400.trig_pos * window ||
             w->start + w->width < - lecroy9400.trig_pos * window )
        {
			print( FATAL, "Window %ld doesn't fit into current digitizer time "
				   "range.\n", w->num );
			THROW( EXCEPTION );
		}

		/* Take care: Numbers start from 1 ! */

		w->start_num = lrnd( ( w->start + lecroy9400.trig_pos * window )
							 * TDS_POINTS_PER_DIV / lecroy9400.timebase ) + 1;
		w->end_num = lrnd( ( w->start + w->width
							 + lecroy9400.trig_pos * window )
							 * TDS_POINTS_PER_DIV / lecroy9400.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			print( FATAL, "Window %ld has width of less than 1 point.\n",
				   w->num );
			THROW( EXCEPTION );
		}
    }
}

#endif


/*--------------------------------------------------------------*/
/* The function is used to translate back and forth between the */
/* channel numbers the way the user specifies them in the EDL   */
/* program and the channel numbers as specified in the header   */
/* file. When the channel number can't be maped correctly, the  */
/* way the function reacts depends on the value of the third    */
/* argument: If this is UNSET, an error message gets printed    */
/* and an exception ios thrown. If it is SET -1 is returned to  */
/* indicate the error.                                          */
/*--------------------------------------------------------------*/

long lecroy9400_translate_channel( int dir, long channel, bool flag )
{
	if ( dir == GENERAL_TO_LECROY9400 )
	{
		switch ( channel )
		{
			case DIGITIZER_CHANNEL_CH1 :
				return LECROY9400_CH1;

			case DIGITIZER_CHANNEL_CH2 :
				return LECROY9400_CH2;

			case DIGITIZER_CHANNEL_MEM_C :
				return LECROY9400_MEM_C;

			case DIGITIZER_CHANNEL_MEM_D :
				return LECROY9400_MEM_D;

			case DIGITIZER_CHANNEL_FUNC_E :
				return LECROY9400_FUNC_E;

			case DIGITIZER_CHANNEL_FUNC_F :
				return LECROY9400_FUNC_F;

			case DIGITIZER_CHANNEL_LINE :
				return LECROY9400_LIN;

			case DIGITIZER_CHANNEL_EXT :
				return LECROY9400_EXT;

			case DIGITIZER_CHANNEL_EXT10 :
				return LECROY9400_EXT10;

			case DIGITIZER_CHANNEL_CH3   :
			case DIGITIZER_CHANNEL_CH4   :
			case DIGITIZER_CHANNEL_MATH1 :
			case DIGITIZER_CHANNEL_MATH2 :
			case DIGITIZER_CHANNEL_MATH3 :
			case DIGITIZER_CHANNEL_REF1  :
			case DIGITIZER_CHANNEL_REF2  :
			case DIGITIZER_CHANNEL_REF3  :
			case DIGITIZER_CHANNEL_REF4  :
			case DIGITIZER_CHANNEL_AUX   :
			case DIGITIZER_CHANNEL_AUX1  :
			case DIGITIZER_CHANNEL_AUX2  :
				if ( flag )
					return -1;
				print( FATAL, "Digitizer has no channel %s.\n",
					   Digitizer_Channel_Names[ channel ] );
				THROW( EXCEPTION );

			default :
				if ( flag )
					return -1;
				print( FATAL, "Invalid channel number %ld.\n", channel );
				THROW( EXCEPTION );
		}
	}
	else
	{
		switch ( channel )
		{
			case LECROY9400_CH1 :
				return DIGITIZER_CHANNEL_CH1;

			case LECROY9400_CH2 :
				return DIGITIZER_CHANNEL_CH2;

			case LECROY9400_MEM_C :
				return DIGITIZER_CHANNEL_MEM_C;

			case LECROY9400_MEM_D :
				return DIGITIZER_CHANNEL_MEM_D;

			case LECROY9400_FUNC_E :
				return DIGITIZER_CHANNEL_FUNC_E;

			case LECROY9400_FUNC_F :
				return DIGITIZER_CHANNEL_FUNC_F;

			case LECROY9400_LIN :
				return DIGITIZER_CHANNEL_LINE;

			case LECROY9400_EXT :
				return DIGITIZER_CHANNEL_EXT;

			case LECROY9400_EXT10 :
				return DIGITIZER_CHANNEL_EXT10;

			default :
				print( FATAL, "Internal error detected at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
		}
	}

	return -1;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

void lecroy9400_store_state( LECROY9400 *dest, LECROY9400 *src )
{
	WINDOW *w;
	int i;


	while ( dest->w != NULL )
	{
		w = dest->w;
		dest->w = w->next;
		T_free( w );
	}

	*dest = *src;

	if ( src->num_windows == 0 )
	{
		dest->w = 0;
		return;
	}

	dest->w = WINDOW_P T_malloc( src->num_windows * sizeof *dest->w );
	for ( i = 0, w = src->w; w != NULL; i++, w = w->next )
	{
		*( dest->w + i ) = *w;
		if ( i != 0 )
			dest->w->prev = dest->w - 1;
		if ( w->next != NULL )
			dest->w->next = dest->w + 1;
	}
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
