/*
  $Id$
*/

#include "tds520.h"



/*-----------------------------------------------------------*/
/* Returns a string with a time value with a resonable unit. */
/*-----------------------------------------------------------*/

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
    double width, window, dcs, dcd, dtb, fac;
    long tb, cs, cd;
	int i;
	char *buffer;


	/* If a trigger channel has been set in the PREPARATIONS section send
	   it to the digitizer */

	if ( tds520.is_trigger_channel )
		tds520_set_trigger_channel( Channel_Names[ tds520.trigger_channel ] );

	/* Switch on all channels that are used in the measurements */

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

	/* If no width is set for all windows get the distance of the cursors on
	   the digitizers screen and use it as the default width. */

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

	/* It's not possible to set arbitrary cursor positions and distances -
	   they've got to be multiples of the smallest time resolution of the
	   digitizer, which is the time base divided by TDS_POINTS_PER_DIV. In the
	   following it is tested if the requested cursor position and distance
	   fit this requirement and if not the values are readjusted. While
	   settings for the position and width of the window not being exact
	   multiples of the resultion are probably no serious errors a window
	   width of less than the resolution is a hint for a real problem. */

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
		cs = lround( TDS_POINTS_PER_DIV * dcs );
		tb = lround( dtb );

		if ( cs % tb )        /* window start not multiple of a point ? */
		{
			cs = ( cs / tb ) * tb;
			dcs = cs * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520_ptime( dcs ) );
			eprint( WARN, "%s: Start point of window %ld had to be readjusted "
					"from %s to %s.\n", DEVICE_NAME, w->num, 
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
		cd = lround( TDS_POINTS_PER_DIV * dcd );
		tb = lround( dtb );

		if ( labs( cd ) < tb )     /* window smaller than one point ? */
		{
			dcd = tds520.timebase / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520_ptime( dcd ) );
			eprint( SEVERE, "%s: Width of window %ld had to be readjusted "
					"from %s to %s.\n", DEVICE_NAME, w->num,
					tds520_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
		else if ( cd % tb )        /* window width not multiple of a point ? */
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			buffer = T_strdup( tds520_ptime( dcd ) );
			eprint( WARN, "%s: Width of window %ld had to be readjusted from "
					"%s to %s.\n", DEVICE_NAME, w->num,
					tds520_ptime( w->width ), buffer );
			T_free( buffer );
			w->width = dcd;
		}
	}

	/* Test if the windows fit into the measurement window and calculate start
	   and end point of window */

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

		/* Take care: Numbers start from 1 ! */

		w->start_num = lround( ( w->start + tds520.trig_pos * window )
							   * TDS_POINTS_PER_DIV / tds520.timebase ) + 1;
		w->end_num = lround( ( w->start + w->width + tds520.trig_pos * window )
							 * TDS_POINTS_PER_DIV / tds520.timebase ) + 1;

		if ( w->end_num - w->start_num <= 0 )
        {
			eprint( FATAL, "%s: Window %ld has width of less than 1 point.\n",
					DEVICE_NAME, w->num );
			THROW( EXCEPTION );
		}
    }
}
