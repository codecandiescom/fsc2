/*
  $Id$
*/

#include "tds520.h"



/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

const char *tds520_ptime( double time )
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
	WINDOW *w, *wn;
	bool is_width = SET;
    double width, window, dcd, dtb, fac;
    long tb, cd;
	int i;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds520.is_trigger_channel )
		tds520_set_trigger_channel( 
			Channel_Names[ tds520.trigger_channel ] );

	/* Switch all channels on that get used in the measurements */

	for ( i = 0; i <= TDS520_REF4; i++)
		if ( tds520.channels_in_use[ i ] )
			tds520_display_channel( i );

	/* Remove all unused windows and test if for all other windows the width
	   is set */

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

		if ( ! w->is_width )
			is_width = UNSET;
		w = w->next;
	}

	/* If not get the distance of the cursors on the digitizers screen and
	   use it as the default width. */

	if ( tds520.w != NULL && ! is_width )
	{
		tds520_get_cursor_distance( &width );

		width = fabs( width );

		if ( width == 0.0 )
		{
			eprint( FATAL, "%s: Can't determine a reasonable value for "
					"still undefined window widths.\n", DEVICE_NAME );
			THROW( EXCEPTION );
		}

		for ( w = tds520.w; w != NULL; w = w->next )
			if ( ! w->is_width )
				w->width = width;
	}

	/* It's not possible to set arbitrary cursor distances - they've got to be
	   multiples of the smallest time resolution of the digitizer, which is
	   1 / TDS_POINTS_PER_DIV of the time base. In the following it is tested
	   if the requested cursor distance fit this requirement and if not the
	   values are readjusted. While we're at it we also try to find out if
	   all window widths are equal - than we can use tracking cursors */

	tds520.is_equal_width = SET;

	for ( w = tds520.w; w != NULL; w = w->next )
	{
		dcd = w->width;
		dtb = tds520.timebase;
		fac = 1.0;

		while ( fabs( dcd ) < 1.0 || fabs( dtb ) < 1.0 )
		{
			dcd *= 1000.0;
			dtb *= 1000.0;
			fac *= 0.001;
		}
		cd = lround( TDS_POINTS_PER_DIV * dcd );
		tb = lround( dtb );

		if ( labs( cd ) < tb )
		{
			w->width = tds520.timebase / TDS_POINTS_PER_DIV;
			eprint( SEVERE, "%s: Width of window %ld has been readjusted to "
					"%s.\n", DEVICE_NAME, w->num, tds520_ptime( w->width  ) );
		}
		else if ( cd % tb )
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			eprint( SEVERE, "%s: Width of window %ld has been readjusted to "
					"%s.\n", DEVICE_NAME, w->num, tds520_ptime( dcd ) );
			w->width = dcd;
		}

		/* Check if the windows have all the same length */

		if ( w != tds520.w && w->width != tds520.w->width )
			tds520.is_equal_width = UNSET;
	}

	/* Next check: Test if the windows fit into the measurement window */

    window = tds520.timebase * tds520.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds520.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds520.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds520.trig_pos ) * window ||
             w->start < - tds520.trig_pos * window ||
             w->start + w->width < - tds520.trig_pos * window )
        {
			eprint( FATAL, "%s: Window %ld doesn't fit into current digitizer "
					"time range.\n", DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}
    }
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520_set_curve_window( WINDOW *w )
{
	tds520_set_window( w );

	if ( w != NULL )
	{
		if ( ! tds520.snap_state )
		{
			tds520_set_snap( SET );
			tds520.snap_state = SET;
		}
	}
	else
	{
		if ( tds520.snap_state )
		{
			tds520_set_snap( UNSET );
			tds520.snap_state = UNSET;
		}
	}
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds520_set_window( WINDOW *w )
{
	if ( w == NULL )
		return;

		/* If all windows have the same width we only have to set the first
		   cursor (and only if its not already at the correct position),
		   otherwise we have to set both cursors */

	if ( tds520.is_equal_width )
	{
		if ( tds520.cursor_pos != w->start )
		{
			tds520_set_cursor( 1, w->start );
			tds520.cursor_pos = w->start;
		}
	}
	else
	{
		tds520_set_cursor( 1, w->start );
		tds520_set_cursor( 2, w->start + w->width );
	}
}
