/*
  $Id$
*/

#include "tds744a.h"


static bool tds744a_window_check_1( void );
static void tds744a_window_check_2( void );
static void tds744a_window_check_3( void );


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

const char *tds744a_ptime( double time )
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

void tds744a_delete_windows( void )
{
	WINDOW *w;

	while ( tds744a.w != NULL )
	{
		w = tds744a.w;
		tds744a.w = w->next;
		T_free( w );
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds744a_do_pre_exp_checks( void )
{
	WINDOW *w;
	bool is_width;
    double width;
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds744a.is_trigger_channel )
		tds744a_set_trigger_channel( 
			Channel_Names[ tds744a.trigger_channel ] );

	/* Switch all channels on that get used in the measurements */

	for ( i = 0; i <= TDS744A_REF4; i++)
		if ( tds744a.channels_in_use[ i ] )
			tds744a_display_channel( i );

	/* Remove all unused windows and test if for all other windows the width
	   is set */

	is_width = tds744a_window_check_1( );

	/* That's all if no windows have been defined we switch off gated
	   measurement mode, i.e. all measurement operations are done on the
	   whole curve */

	if ( tds744a.w == NULL )
	{
		tds744a_set_gated_meas( UNSET );
		tds744a.gated_state = UNSET;
		return;
	}

	/* If not get the distance of the cursors on the digitizers screen and
	   use it as the default width. */

	if ( tds744a.w != NULL && ! is_width )
	{
		tds744a_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			eprint( FATAL, "%s: Can't determine a reasonable value for "
					"the missing window widths.\n", DEVICE_NAME );
			THROW( EXCEPTION );
		}

		for ( w = tds744a.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursorsd can be positioned exactly
	   and are still within the range of the digitizers record length */

	tds744a_window_check_2( );
	tds744a_window_check_3( );

	/* Now that all windows are properly set we switch on gated measurements */

	tds744a_set_gated_meas( SET );
	tds744a.gated_state = SET;

	/* If the widths of all windows are equal we switch on tracking cursor
	   mode and set the cursors to the position of the first window */

	if ( tds744a.is_equal_width )
	{
		tds744a_set_cursor( 1, tds744a.w->start );
		tds744a_set_cursor( 2, tds744a.w->start + tds744a.w->width );
		tds744a_set_track_cursors( SET );
		tds744a.cursor_pos = tds744a.w->start;
	}
	else
		tds744a_set_track_cursors( UNSET );
}


/*---------------------------------------------------------------*/
/* Removes unused windows and checks if for all the used windows */
/* a width is set - this is returned to the calling function     */
/*---------------------------------------------------------------*/

bool tds744a_window_check_1( void )
{
	WINDOW *w, *wn;
	bool is_width = SET;


	for ( w = tds744a.w; w != NULL; )
	{
		if ( ! w->is_used )
		{
			if ( w == tds744a.w )
				wn = tds744a.w = w->next;
			else
				wn = w->prev->next = w->next;

			T_free( w );
			tds744a.num_windows--;
			w = wn;
			continue;
		}

		if ( ! w->is_width )
			is_width = UNSET;
		w = w->next;
	}

	return is_width;
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

void tds744a_window_check_2( void )
{
	WINDOW *w;
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	tds744a.is_equal_width = SET;

	for ( w = tds744a.w; w != NULL; w = w->next )
	{
		dcs = w->start;
		dtb = tds744a.timebase;
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
			buffer = T_strdup( tds744a_ptime( dcs ) );
			eprint( WARN, "%s: Start point of window %ld had to be readjusted "
					"from %s to %s.\n", DEVICE_NAME, w->num,
					tds744a_ptime( w->start ), buffer );
			T_free( buffer );
			w->start = dcs;
		}

		dcd = w->width;
		dtb = tds744a.timebase;
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
			dcd = tds744a.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds744a_ptime( dcd ) );
			eprint( SEVERE, "%s: Width of window %ld had to be readjusted "
					"from %s to %s.\n", DEVICE_NAME, w->num,
					tds744a_ptime( w->width  ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds744a_ptime( dcd ) );
			eprint( WARN, "%s: Width of window %ld had to be readjusted from "
					"%s to %s.\n", DEVICE_NAME, w->num,
					tds744a_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}

		/* Check if the windows have all the same length */

		if ( w != tds744a.w && w->width != tds744a.w->width )
			tds744a.is_equal_width = UNSET;
	}
}


/*-------------------------------------------------------------*/
/* This function checks if the windows fit into the digitizers */
/* measurement window and calculate the positions of the start */
/* and the end of the windows in units of points.              */
/*-------------------------------------------------------------*/

void tds744a_window_check_3( void )
{
	WINDOW *w;
    double window;


    window = tds744a.timebase * tds744a.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds744a.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds744a.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds744a.trig_pos ) * window ||
             w->start < - tds744a.trig_pos * window ||
             w->start + w->width < - tds744a.trig_pos * window )
        {
			eprint( FATAL, "%s: Window %ld doesn't fit into current digitizer "
					"time range.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}

		/* Take care: Numbers start from 1 ! */

		w->start_num = lround( ( w->start + tds744a.trig_pos * window )
							   * TDS_POINTS_PER_DIV / tds744a.timebase ) + 1;
		w->end_num = lround( ( w->start + w->width
							   + tds744a.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds744a.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			eprint( FATAL, "%s: Window %ld has width of less than 1 point.\n",
					DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}
    }
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds744a_set_meas_window( WINDOW *w )
{
	tds744a_set_window( w );

	if ( w != NULL )
	{
		/* If not already in gated measurement state set it now */

		if ( ! tds744a.gated_state )
		{
			tds744a_set_gated_meas( SET );
			tds744a.gated_state = SET;
		}
	}
	else
	{
		/* If in gated measurement state switch it off */

		if ( tds744a.gated_state )
		{
			tds744a_set_gated_meas( UNSET );
			tds744a.gated_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds744a_set_curve_window( WINDOW *w )
{
	tds744a_set_window( w );

	if ( w != NULL )
	{
		if ( ! tds744a.snap_state )
		{
			tds744a_set_snap( SET );
			tds744a.snap_state = SET;
		}
	}
	else
	{
		if ( tds744a.snap_state )
		{
			tds744a_set_snap( UNSET );
			tds744a.snap_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds744a_set_window( WINDOW *w )
{
	if ( w == NULL )
		return;

	/* If all windows have the same width we only have to set the first
	   cursor (and only if its not already at the correct position),
	   otherwise we have to set both cursors */

	if ( tds744a.is_equal_width )
	{
		if ( tds744a.cursor_pos != w->start )
		{
			tds744a_set_cursor( 1, w->start );
			tds744a.cursor_pos = w->start;
		}
	}
	else
	{
		tds744a_set_cursor( 1, w->start );
		tds744a_set_cursor( 2, w->start + w->width );
	}
}
