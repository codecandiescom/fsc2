/*
  $Id$
*/

#include "tds754a.h"


static void tds754a_window_check_1( bool *is_start, bool *is_width );
static void tds754a_window_check_2( void );
static void tds754a_window_check_3( void );


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

const char *tds754a_ptime( double time )
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

void tds754a_delete_windows( void )
{
	WINDOW *w;

	while ( tds754a.w != NULL )
	{
		w = tds754a.w;
		tds754a.w = w->next;
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
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds754a.is_trigger_channel )
		tds754a_set_trigger_channel( 
			Channel_Names[ tds754a.trigger_channel ] );

	/* Switch all channels on that get used in the measurements */

	for ( i = 0; i <= TDS754A_REF4; i++)
		if ( tds754a.channels_in_use[ i ] )
			tds754a_display_channel( i );

	/* Remove all unused windows and test if for all other windows the start
	   position and the width is set */

	tds754a_window_check_1( &is_start, &is_width );

	/* That's all if no windows have been defined we switch off gated
	   measurement mode, i.e. all measurement operations are done on the
	   whole curve */

	if ( tds754a.w == NULL )
	{
		tds754a_set_gated_meas( UNSET );
		tds754a.gated_state = UNSET;
		return;
	}

	/* If start position isn't set for all windows set it to the position of
	   the left cursor */

	if ( ! is_start )
		for ( w = tds754a.w; w != NULL; w = w->next )
			if ( ! w->is_start )
			{
				w->start = tds754a.cursor_pos;
				w->is_start = SET;
			}

	/* If not get the distance of the cursors on the digitizers screen and
	   use it as the default width. */

	if ( ! is_width )
	{
		tds754a_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			eprint( FATAL, UNSET, "%s: Can't determine a reasonable value for "
					"the missing window widths.\n", DEVICE_NAME );
			THROW( EXCEPTION );
		}

		for ( w = tds754a.w; w != NULL; w = w->next )
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
	}

	/* Make sure the windows are ok, i.e. cursorsd can be positioned exactly
	   and are still within the range of the digitizers record length */

	tds754a_window_check_2( );
	tds754a_window_check_3( );

	/* Now that all windows are properly set we switch on gated measurements */

	tds754a_set_gated_meas( SET );
	tds754a.gated_state = SET;

	/* If the widths of all windows are equal we switch on tracking cursor
	   mode and set the cursors to the position of the first window */

	if ( tds754a.is_equal_width )
	{
		tds754a_set_cursor( 1, tds754a.w->start );
		tds754a_set_cursor( 2, tds754a.w->start + tds754a.w->width );
		tds754a_set_track_cursors( SET );
		tds754a.cursor_pos = tds754a.w->start;
	}
	else
		tds754a_set_track_cursors( UNSET );
}


/*---------------------------------------------------------------*/
/* Removes unused windows and checks if for all the used windows */
/* a width is set - this is returned to the calling function     */
/*---------------------------------------------------------------*/

static void tds754a_window_check_1( bool *is_start, bool *is_width )
{
	WINDOW *w, *wn;


	*is_start = *is_width = SET;

	for ( w = tds754a.w; w != NULL; )
	{
		if ( ! w->is_used )
		{
			if ( w == tds754a.w )
				wn = tds754a.w = w->next;
			else
				wn = w->prev->next = w->next;

			T_free( w );
			tds754a.num_windows--;
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

static void tds754a_window_check_2( void )
{
	WINDOW *w;
    double dcs, dcd, dtb, fac;
    long tb, cs, cd;
	char *buffer;


	tds754a.is_equal_width = SET;

	for ( w = tds754a.w; w != NULL; w = w->next )
	{
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
		cs = lround( TDS_POINTS_PER_DIV * dcs );
		tb = lround( dtb );

		if ( cs % tb )        /* window start not multiple of a point ? */
		{
			cs = ( cs / tb ) * tb;
			dcs = cs * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds754a_ptime( dcs ) );
			eprint( WARN, UNSET, "%s: Start point of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds754a_ptime( w->start ), buffer );
			T_free( buffer );
			w->start = dcs;
		}

		dcd = w->width;
		dtb = tds754a.timebase;
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
			dcd = tds754a.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds754a_ptime( dcd ) );
			eprint( SEVERE, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds754a_ptime( w->width  ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds754a_ptime( dcd ) );
			eprint( WARN, UNSET, "%s: Width of window %ld had to be "
					"readjusted from %s to %s.\n", DEVICE_NAME, w->num,
					tds754a_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}

		/* Check if the windows have all the same length */

		if ( w != tds754a.w && w->width != tds754a.w->width )
			tds754a.is_equal_width = UNSET;
	}
}


/*-------------------------------------------------------------*/
/* This function checks if the windows fit into the digitizers */
/* measurement window and calculate the positions of the start */
/* and the end of the windows in units of points.              */
/*-------------------------------------------------------------*/

static void tds754a_window_check_3( void )
{
	WINDOW *w;
    double window;


    window = tds754a.timebase * tds754a.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds754a.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds754a.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds754a.trig_pos ) * window ||
             w->start < - tds754a.trig_pos * window ||
             w->start + w->width < - tds754a.trig_pos * window )
        {
			eprint( FATAL, UNSET, "%s: Window %ld doesn't fit into current "
					"digitizer time range.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}

		/* Take care: Numbers start from 1 ! */

		w->start_num = lround( ( w->start + tds754a.trig_pos * window )
							   * TDS_POINTS_PER_DIV / tds754a.timebase ) + 1;
		w->end_num = lround( ( w->start + w->width
							   + tds754a.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds754a.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			eprint( FATAL, UNSET, "%s: Window %ld has width of less than 1 "
					"point.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}
    }
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

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


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

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


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

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
}
