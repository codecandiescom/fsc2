/*
  $Id$

  Copyright (C) 1999-2004 Jens Thoms Toerring

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


#include "fsc2.h"


static void	unpack_and_accept( int dim, char *ptr );
static void	other_data_request( int dim, int type, char *ptr );
static void accept_1d_data( long x_index, long curve, int type, char *ptr );
static void accept_1d_data_sliding( long curve, int type, char *ptr );
static void accept_2d_data( long x_index, long y_index, long curve, int type,
							char *ptr );
static long get_number_of_new_points( char **ptr, int type );
static bool get_new_extrema( double *max, double *min, char *ptr,
							 long len, int type );
static bool	incr_x( long x_index, long len );
static bool	incr_y( long y_index );
static bool	incr_x_and_y( long x_index, long len, long y_index );


/* These variables are used to determine which parts of the display(s) need
   redrawing after the data have been unpacked in order to minimize the
   number of graphical operations */

static bool scale_1d_changed[ 2 ];
static bool scale_2d_changed[ 3 ];
static bool need_2d_redraw;
static bool need_cut_redraw;


#define MAX_ACCEPT_TIME  0.2    /* 200 ms */


/*--------------------------------------------------------------------------*/
/* This is the function that takes the new data from the message queue and  */
/* displays them. The function is invoked as a idle callback, i.e. whenever */
/* the program has some unused time. For a limited time (currently set to   */
/* 200 ms) and schedules them to be displayed. Data sets for REQUEST type   */
/* data are moved to the end of the message queue (there can only be one at */
/* a time because REQUESTS need a reply to the child process). When all     */
/* sets have been removed from the message queue (or the maximum time in    */
/* the accept loop is over) all parts of the display windows that changed   */
/* due to the new data get udated.                                          */
/*--------------------------------------------------------------------------*/

void accept_new_data( bool empty_queue )
{
	char *buf;
	int type;
	int dim = 0;
	struct timeval time_struct;
	double start_time = 0.0;


	CLOBBER_PROTECT( dim );
	CLOBBER_PROTECT( start_time );

	/* Get the time we arrived here, it's later used to avoid spending too
	   much time in the loop for accepting data sets */

	if ( ! empty_queue )
	{
		gettimeofday( &time_struct, NULL );
		start_time = time_struct.tv_sec + 1.0e-6 * time_struct.tv_sec;
	}

	/* Clear the flags that later tell us what really needs to be redrawn */

	memset( scale_1d_changed, 0, sizeof scale_1d_changed );
	memset( scale_2d_changed, 0, sizeof scale_2d_changed );
	need_2d_redraw = need_cut_redraw = UNSET;

	while ( 1 )
	{
		/* Attach to the shared memory segment pointed to by the oldest entry
		   in the message queue - even though this should never fail it
		   sometimes does (seems to be 2.0 kernels only) so we better have a
		   bit more of error output until this is finally sorted out. */

		if ( ( buf =
			    ( char * ) attach_shm( Comm.MQ->slot[ Comm.MQ->low ].shm_id ) )
			 == ( char * ) - 1 )
		{
#ifndef NDEBUG
			eprint( FATAL, UNSET, "Internal communication error at %s:%d, "
					"message_queue_low = %d, shm_id = %d.\n"
					"*** PLEASE SEND A BUG REPORT CITING THESE LINES *** "
					"Thank you.\n",
					__FILE__, __LINE__, Comm.MQ->low,
					Comm.MQ->slot[ Comm.MQ->low ].shm_id );
#else
			eprint( FATAL, UNSET, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
#endif
			THROW( EXCEPTION );
		}

		/* Unpack and accept the data sets (skip the length field, it's not
		   needed) - if an exception happens detach from the segment and
		   re-throw the exception */

		TRY
		{
			type = Comm.MQ->slot[ Comm.MQ->low ].type;
			dim |= type == DATA_1D ? 1 : 2;
			unpack_and_accept( type, buf + sizeof( long ) );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			detach_shm( buf, &Comm.MQ->slot[ Comm.MQ->low ].shm_id );
			RETHROW( );
		}

		/* Detach from shared memory segment and remove it */

		detach_shm( buf, &Comm.MQ->slot[ Comm.MQ->low ].shm_id );
		Comm.MQ->slot[ Comm.MQ->low ].shm_id = -1;

		/* Increment the low queue pointer and post the data semaphore to
		   tell the child that there's again room in the message queue. */

		Comm.MQ->low = ( Comm.MQ->low + 1 ) % QUEUE_SIZE;
		sema_post( Comm.mq_semaphore );

		/* Return if all entries in the message queue are used up or there's
		   a REQUEST (which is always the last entry because the child has to
		   wait for a reply and which gets handled by the calling routine) */

		if ( Comm.MQ->low == Comm.MQ->high ||
			 Comm.MQ->slot[ Comm.MQ->low ].type == REQUEST )
			break;

		/* Also quit from the loop at least about every fifth of a second 
		   unless 'empty_queue' is set, in which case the child is dead and
		   we have to fetch everything it send during its live time. */

		if ( ! empty_queue )
		{
			gettimeofday( &time_struct, NULL );
			if ( time_struct.tv_sec + 1.0e-6 * time_struct.tv_sec
				 - start_time >= MAX_ACCEPT_TIME )
				break;
		}
  	}

	/* After being done with unpacking we display the new data by redrawing
	   the canvases that have been changed because of the new data */

	if ( dim & 1 )
	{
		redraw_canvas_1d( &G1.canvas );
		if ( scale_1d_changed[ X ] )
			redraw_canvas_1d( &G1.x_axis );
		if ( scale_1d_changed[ Y ] )
			redraw_canvas_1d( &G1.y_axis );
	}

	if ( dim & 2 )
	{
		if ( need_2d_redraw )
			redraw_canvas_2d( &G2.canvas );
		if ( scale_2d_changed[ X ] )
			redraw_canvas_2d( &G2.x_axis );
		if ( scale_2d_changed[ Y ] )
			redraw_canvas_2d( &G2.y_axis );
		if ( scale_2d_changed[ Z ] )
			redraw_canvas_2d( &G2.z_axis );
		if ( need_cut_redraw )
			redraw_all_cut_canvases( );
	}
}


/*------------------------------------------------------------------*/
/* This function examines the new data by looking at the first item */
/* and calls the appropriate functions for dealing with the data.   */
/*------------------------------------------------------------------*/

static void unpack_and_accept( int dim, char *ptr )
{
	int i;
	int nsets;
	char *ptr_next = NULL;
	long x_index,
		 y_index,
		 curve;
	int type;
	long len;


	/* The first item of a new data package indicates the amount of data
	   sets that needs handling. When the first item is a negative number
	   it isn't data to be displayed but special commands for clearing
	   curves, scale changes etc. */

	memcpy( &nsets, ptr, sizeof nsets );
	ptr += sizeof nsets;

	if ( nsets < 0 )                /* special for clear curve commands etc. */
	{
		other_data_request( dim, nsets, ptr );
		return;
	}

	/* Accept the new data for the different types of data sets */

	for ( i = 0; i < nsets; ptr = ptr_next, i++ )
	{
		memcpy( &x_index, ptr, sizeof x_index );
		ptr += sizeof x_index;

		if ( dim == DATA_2D )
		{
			memcpy( &y_index, ptr, sizeof y_index );
			ptr += sizeof y_index;
		}

		memcpy( &curve, ptr, sizeof curve );
		ptr += sizeof curve;

		memcpy( &type, ptr, sizeof type );
		ptr += sizeof type;

		switch ( type )
		{
			case INT_VAR :
				ptr_next = ptr + sizeof( long );
				break;

			case FLOAT_VAR :
				ptr_next = ptr + sizeof( double );
				break;

			case INT_ARR :
				memcpy( &len, ptr, sizeof len );
				ptr_next = ptr + sizeof len + len * sizeof( long );
				break;

			case FLOAT_ARR :
				memcpy( &len, ptr, sizeof len );
				ptr_next = ptr + sizeof len + len * sizeof( double );
				break;

			case INT_REF : case FLOAT_REF :
				fsc2_assert( dim == DATA_2D );
				memcpy( &len, ptr, sizeof len );
				ptr += sizeof len;
				ptr_next = ptr + len;
				break;

#ifndef NDEBUG
			default :
				eprint( FATAL, UNSET, "Internal communication error at "
						"%s:%d.\n", __FILE__, __LINE__ );
				THROW( EXCEPTION );
#endif
		}

		if ( dim == DATA_1D )
			accept_1d_data( x_index, curve, type, ptr );
		else
			accept_2d_data( x_index, y_index, curve, type, ptr );
	}
}


/*-------------------------------------------------------------------------*/
/* Function for handling special commands that come disguised as data like */
/* commands for clearing curves, changing scales etc. Here the type of the */
/* command is determined and the appropriate functions are called.         */
/*-------------------------------------------------------------------------*/

static void other_data_request( int dim, int type, char *ptr )
{
	long i;
	long count;
	long ca;
	int is_set;
	char *label[ 3 ];
	long lengths[ 3 ];
	long position;
	long color;
	long mode;
	long width;
	long x_pos;
	long y_pos;


	switch( type )
	{
		case D_CLEAR_CURVE :                     /* clear curve command */
			memcpy( &count, ptr, sizeof count ); /* length of list of curves */
			ptr += sizeof count;

			for ( i = 0; i < count; ptr += sizeof ca, i++ )
			{
				memcpy( &ca, ptr, sizeof ca );    /* current curve number */
				if ( dim == DATA_1D )
					clear_curve_1d( ca );
				else
				{
					clear_curve_2d( ca );
					need_2d_redraw = SET;
					cut_clear_curve( ca );
					need_cut_redraw = SET;
				}
			}

			break;

		case D_CHANGE_SCALE :                       /* change scale command */
			memcpy( &is_set, ptr, sizeof is_set );  /* flags */
			ptr += sizeof is_set;
			if ( dim == DATA_1D )
			{
				change_scale_1d( is_set, ( void * ) ptr );
				scale_1d_changed[ X ] = scale_1d_changed[ Y ] = SET;
			}
			else
			{
				change_scale_2d( is_set, ( void * ) ptr );
				scale_2d_changed[ X ] =
					scale_2d_changed[ Y ] = 
						scale_2d_changed[ Z ] = SET;
			}
			break;

		case D_CHANGE_LABEL :                 /* change label command */
			for ( i = X; i <= ( dim == DATA_1D ? Y : Z ); i++ )
			{
				memcpy( lengths + i, ptr, sizeof lengths[ i ] );
				ptr += sizeof lengths[ i ];
				label[ i ] = ( char * ) ptr;
				ptr += lengths[ i ];
			}
			if ( dim == DATA_1D )
			{
				change_label_1d( label );
				scale_1d_changed[ X ] = scale_1d_changed[ Y ] = SET;
			}
			else
			{
				change_label_2d( label );
				scale_2d_changed[ X ] =
					scale_2d_changed[ Y ] = 
						scale_2d_changed[ Z ] = SET;
			}
			break;

		case D_CHANGE_POINTS :                /* rescale command */
			if ( dim == DATA_1D )
				rescale_1d( * ( long * ) ptr );
			else
			{
				rescale_2d( ( long * ) ptr );
				need_2d_redraw = scale_2d_changed[ X ] =
					scale_2d_changed[ Y ] = SET;
			}
			break;

		case D_SET_MARKER :
			if ( dim == DATA_1D )
			{
				memcpy( &position, ptr, sizeof position );
				ptr += sizeof position;
				memcpy( &color, ptr, sizeof color );
				if ( position >= G1.nx && G.mode == NORMAL_DISPLAY )
					rescale_1d( position );
				set_marker_1d( position, color );
			}
			else
			{
				memcpy( &x_pos, ptr, sizeof x_pos );
				ptr += sizeof x_pos;
				memcpy( &y_pos, ptr, sizeof y_pos );
				ptr += sizeof position;
				memcpy( &color, ptr, sizeof color );
				ptr += sizeof color;
				memcpy( &count, ptr, sizeof count );
				set_marker_2d( x_pos, y_pos, color, count );
				need_2d_redraw = SET;
			}
			break;

		case D_CLEAR_MARKERS :
			if ( dim == DATA_1D )
				remove_markers_1d( );
			else
			{
				remove_markers_2d( ( long * ) ptr );
				need_2d_redraw = SET;
			}
			break;

		case D_CHANGE_MODE :
			fsc2_assert( dim == DATA_1D );
			memcpy( &mode, ptr, sizeof mode );
			ptr +=sizeof mode;
			memcpy( &width, ptr, sizeof width );
			change_mode( mode, width );
			break;

#ifndef NDEBUG
		default :                             /* unknown command */
			eprint( FATAL, UNSET, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
#endif
	}
}


/*--------------------------------------------------------*/
/* function for storing and displaying of new data points */
/* for the 1D graphic (when normal display mode is used). */
/*--------------------------------------------------------*/

static void accept_1d_data( long x_index, long curve, int type, char *ptr )
{
	long len = 0;
	char *cur_ptr;
	double data;
	long ldata;
	double new_rwc_delta_y;
	double old_rw_min;
	double fac, off;
	Curve_1d *cv;
	long i, j;
	Scaled_Point *sp;
	long count;


	/* Test if the curve number is OK */

#ifndef NDEBUG
	if ( curve >= G1.nc )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d, there is no "
				"curve %ld.\n", __FILE__, __LINE__, curve + 1 );
		THROW( EXCEPTION );
	}
#endif

	/* If we're in SLIDING WINDOW mode call the function for displaying the
	   new data in this mode */

	if ( G.mode == SLIDING_DISPLAY )
	{
		accept_1d_data_sliding( curve, type, ptr );
		return;
	}

	/* Get the amount of new data and a pointer to the start of the data */

	len = get_number_of_new_points( &ptr, type );

	/* If the number of points exceeds the size of the arrays for the curves
	   we have to increase the sizes for all curves. But take care: While
	   x_index + len may be greater than G1.nx, x_index can still be smaller
	   than G1.nx ! */

	if ( x_index + len > G1.nx )
	{
		for ( i = 0, cv = G1.curve[ i ]; i < G1.nc; i++, cv = G1.curve[ i ] )
		{
			cv->points = SCALED_POINT_P T_realloc( cv->points,
								      ( x_index + len ) * sizeof *cv->points );
			cv->xpoints = XPOINT_P T_realloc( cv->xpoints,
									 ( x_index + len ) * sizeof *cv->xpoints );

			for ( j = G1.nx, sp = cv->points + j;
				  j < x_index + len; sp++, j++ )
				sp->exist = UNSET;

			if ( G1.is_fs )
				cv->s2d[ X ] = ( double ) ( G1.canvas.w - 1 ) /
							   ( double ) ( x_index + len - 1 );
		}

		scale_1d_changed[ X ] |= G1.is_fs;
	}

	/* Find maximum and minimum of old and new data and, if the minimum or
       maximum has changed, rescale all old scaled data*/

	old_rw_min = G1.rw_min;

	if ( get_new_extrema( &G1.rw_max, &G1.rw_min, ptr, len, type ) )
	{
		new_rwc_delta_y = G1.rw_max - G1.rw_min;

		if ( G1.is_scale_set )
		{
			fac = G1.rwc_delta[ Y ] / new_rwc_delta_y;
			off = ( old_rw_min - G1.rw_min ) / new_rwc_delta_y;

			for ( i = 0, cv = G1.curve[ i ]; i < G1.nc; cv = G1.curve[ ++i ] )
			{
				for ( sp = cv->points, count = cv->count;
					  count > 0; sp++ )
					if ( sp->exist )
					{
						sp->v = fac * sp->v + off;
						count--;
					}

				if ( G1.is_fs )
					continue;

				cv->s2d[ Y ]  /= fac;
				cv->shift[ Y ] = fac * cv->shift[ Y ] - off;
			}
		}

		/* If the data had not been scaled to [0,1] yet and the maximum value
		   isn't identical to the minimum anymore do the scaling now */

		if ( ! G1.is_scale_set && G1.rw_max != G1.rw_min )
		{
			fac = 1.0 / new_rwc_delta_y;
			off = - G1.rw_min / new_rwc_delta_y;

			for ( i = 0, cv = G1.curve[ i ]; i < G1.nc; cv = G1.curve[ ++i ] )
				for ( sp = cv->points, count = cv->count; count > 0; sp++ )
					if ( sp->exist )
					{
						  sp->v = fac * sp->v + off;
						  count--;
					}

			G1.is_scale_set = SET;
			scale_1d_changed[ X ] = SET;
		}

		scale_1d_changed[ Y ] = SET;
		G1.rwc_delta[ Y ] = new_rwc_delta_y;
	}

	/* Now we're finished with rescaling and can set the new number of points
	   if necessary */

	if ( x_index + len > G1.nx )
		G1.nx = x_index + len;

	/* Include the new data into the scaled data */

	for ( cur_ptr = ptr, i = x_index, sp = G1.curve[ curve ]->points + x_index;
		  i < x_index + len; sp++, i++ )
	{
		if ( type & ( INT_VAR | INT_ARR ) )
		{
			memcpy( &ldata, cur_ptr, sizeof ldata );
			data = ( double ) ldata;
			cur_ptr += sizeof ldata;
		}
		else
		{
			memcpy( &data, cur_ptr, sizeof data );
			cur_ptr += sizeof data;
		}

		if ( G1.is_scale_set )
			sp->v = ( data - G1.rw_min ) / G1.rwc_delta[ Y ];
		else
			sp->v = data;

		/* Increase the point count if the point is new and mark it as set */

		if ( ! sp->exist )
		{
			G1.curve[ curve ]->count++;
			sp->exist = SET;
		}
	}

	/* Calculate new points for display */

	if ( ! G1.is_scale_set )
		return;

	G1.rwc_start[ Y ] = G1.rw_min;

	/* If the scale did not change redraw only the current curve, otherwise all
	   curves */

	if ( ! ( scale_1d_changed[ X ] || scale_1d_changed[ Y ] ) )
		recalc_XPoints_of_curve_1d( G1.curve[ curve ] );
	else
		for ( i = 0; i < G1.nc; i++ )
			recalc_XPoints_of_curve_1d( G1.curve[ i ] );
}


/*--------------------------------------------------------*/
/* Function for handling new data for the 1D display when */
/* "sliding window" mode is used.                         */
/*--------------------------------------------------------*/

static void accept_1d_data_sliding( long curve, int type, char *ptr )
{
	long len = 0;
	char *cur_ptr;
	long ldata;
	double data;
	double new_rwc_delta_y;
	double old_rw_min;
	double fac, off;
	Curve_1d *cv;
	long i;
	Scaled_Point *sp, *sp1, *sp2;
	long count;
	long shift;
	Marker_1D *m, *mn, *mp;


	/* Get the amount of new data and a pointer to the start of the data */

	len = get_number_of_new_points( &ptr, type );

	/* Find maximum and minimum of old and new data and, if the minimum or
       maximum has changed, rescale all old scaled data*/

	old_rw_min = G1.rw_min;

	if ( get_new_extrema( &G1.rw_max, &G1.rw_min, ptr, len, type ) )
	{
		new_rwc_delta_y = G1.rw_max - G1.rw_min;

		if ( G1.is_scale_set )
		{
			fac = G1.rwc_delta[ Y ] / new_rwc_delta_y;
			off = ( old_rw_min - G1.rw_min ) / new_rwc_delta_y;

			for ( i = 0, cv = G1.curve[ i ]; i < G1.nc; cv = G1.curve[ ++i ] )
			{
				for ( sp = cv->points, count = cv->count; count > 0; sp++ )
				{
					sp->v = fac * sp->v + off;
					count--;
				}

				if ( G1.is_fs )
					continue;

				cv->s2d[ Y ]  /= fac;
				cv->shift[ Y ] = fac * cv->shift[ Y ] - off;
			}
		}

		/* If the data had not been scaled to [0,1] yet and the maximum value
		   isn't identical to the minimum anymore do the scaling now */

		if ( ! G1.is_scale_set && G1.rw_max != G1.rw_min )
		{
			fac = 1.0 / new_rwc_delta_y;
			off = - G1.rw_min / new_rwc_delta_y;

			for ( i = 0, cv = G1.curve[ i ]; i < G1.nc; cv = G1.curve[ ++i ] )
				for ( sp = cv->points, count = cv->count; count > 0;
					  sp++, count-- )
					sp->v = fac * sp->v + off;

			scale_1d_changed[ X ] = SET;
			G1.is_scale_set = SET;
		}


		scale_1d_changed[ Y ] = SET;
		G1.rwc_delta[ Y ] = new_rwc_delta_y;
	}

	/* Now we're finished with rescaling we can deal with the new data. First
	   we must drop new points that wouldn't fit into the window (in case that
	   we get too many new points). */

	if ( len > G1.nx )
	{
		fsc2_assert( type ==  INT_ARR || type == FLOAT_ARR );
		ptr += ( len - G1.nx )
			   * ( type == INT_ARR ? sizeof( long ) : sizeof( double ) );
		len = G1.nx;
	}

	/* Remove old points of all curves that wouldn't fit into the window
	   anymore */

	if ( G1.curve[ curve ]->count + len > G1.nx )
	{
		shift = len + G1.curve[ curve ]->count - G1.nx;

		for ( i = 0; i < G1.nc; i++ )
		{
			cv = G1.curve[ i ];
			if ( shift >= cv->count )
			{
				for ( sp = cv->points, count = cv->count; count > 0;
					  sp++, count-- )
					sp->exist = UNSET;
				cv->count = 0;
			}
			else
			{
				for ( sp1 = cv->points, sp2 = sp1 + shift, count = shift;
					  count < cv->count; sp1++, sp2++, count++ )
					sp1->v = sp2->v;
				for ( count -= shift, sp2 -= shift; count < cv->count;
					  sp2++, count++ )
					sp2->exist = 0;

				cv->count -= shift;
			}
		}

		for ( mp = NULL, m = G1.marker_1d; m != NULL; )
		{
			m->x_pos -= shift;
			if ( m->x_pos < 0 )
			{
				XFreeGC( G.d, m->gc );
				mn = m->next;
				if ( mp != NULL )
					mp->next = mn;
				if ( m == G1.marker_1d )
					G1.marker_1d = mn;
				T_free( m );
				m = mn;
			}
			else
			{
				mp = m;
				m = m->next;
			}
		}
	}

	/* Now append the new data */

	cv = G1.curve[ curve ];
	for ( cur_ptr = ptr, i = 0, sp = cv->points + cv->count;
		  i < len; sp++, i++ )
	{
		if ( type & ( INT_VAR | INT_ARR ) )
		{
			memcpy( &ldata, cur_ptr, sizeof ldata );
			data = ( double ) ldata;
			cur_ptr += sizeof ldata;
		}
		else
		{
			memcpy( &data, cur_ptr, sizeof data );
			cur_ptr += sizeof data;
		}

		if ( G1.is_scale_set )
			sp->v = ( data - G1.rw_min ) / G1.rwc_delta[ Y ];
		else
			sp->v = data;

		sp->exist = SET;
	}

	cv->count += len;

	/* Calculate new points for display */

	if ( ! G1.is_scale_set )
		return;

	G1.rwc_start[ Y ] = G1.rw_min;

	/* If the scale did not change recalculate the points of the current curve
	   only, otherwise the points of all curves */

	if ( ! ( scale_1d_changed[ X ] || scale_1d_changed[ Y ] ) )
		recalc_XPoints_of_curve_1d( G1.curve[ curve ] );
	else
		for ( i = 0; i < G1.nc; i++ )
			recalc_XPoints_of_curve_1d( G1.curve[ i ] );
}



/*----------------------------------------------------------------*/
/* Function for storing and displaying of new data points for the */
/* 2D display (sorry if it's a bit hard to understand, but quite  */
/* a bit of work has gone into getting the function to make it as */
/* fast as possible...)                                           */
/*----------------------------------------------------------------*/

static void accept_2d_data( long x_index, long y_index, long curve, int type,
							char *ptr )
{
	long x_len = 0;
	long y_len = 0;
	long count;
	char *cur_ptr;
	double data;
	long ldata;
	double new_rwc_delta_z, fac, off, old_rw_min;
	Curve_2d *cv;
	long i, j;
	Scaled_Point *sp;
	bool size_changed = UNSET;


	/* Test if the curve number is OK */

#ifndef NDEBUG
	if ( curve >= G2.nc )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d, there is no "
				"curve %ld.\n", __FILE__, __LINE__, curve + 1 );
		THROW( EXCEPTION );
	}
#endif

	/* Get number of new data points and pointer to the start of the data */

	x_len = get_number_of_new_points( &ptr, type );

	if ( type & ( INT_REF | FLOAT_REF ) )
	{
		memcpy( &y_len, ptr, sizeof y_len );
		y_len--;
	}

	cv = G2.curve_2d[ curve ];

	/* Check if the new data fit into the already allocated memory, otherwise
	   extend the memory area and figure out if this requires redrawing of the
	   areas for the axes and the cut window. */

	if ( x_index + x_len > G2.nx )
	{
		if ( y_index + y_len >= G2.ny )
		{
			need_cut_redraw |= incr_x_and_y( x_index, x_len, y_index + y_len);
			if ( ( cv->active && cv->is_fs ) ||
				 ( G2.active_curve != -1 &&
				   G2.curve_2d[ G2.active_curve ]->is_fs ) )
				scale_2d_changed[ X ] = scale_2d_changed[ Y ] = SET;
		}
		else
		{
			need_cut_redraw |= incr_x( x_index, x_len );
			scale_2d_changed[ X ] |= ( cv->active && cv->is_fs ) ||
									 ( G2.active_curve != -1 &&
									   G2.curve_2d[ G2.active_curve ]->is_fs );
		}

		size_changed = SET;
	}
	else if ( y_index + y_len >= G2.ny )
	{
		need_cut_redraw |= incr_y( y_index + y_len );
		scale_2d_changed[ Y ] |= ( cv->active && cv->is_fs  ) ||
								 ( G2.active_curve != -1 &&
								   G2.curve_2d[ G2.active_curve ]->is_fs );
		size_changed = SET;
	}

	/* Find maximum and minimum of old and new data and, if the minimum or
	   maximum changed, (re)scale all old data */

	old_rw_min = cv->rw_min;

	if ( get_new_extrema( &cv->rw_max, &cv->rw_min, ptr, x_len, type ) )
	{
		new_rwc_delta_z = cv->rw_max - cv->rw_min;

		if ( cv->is_scale_set )
		{
			fac = cv->rwc_delta[ Z ] / new_rwc_delta_z;
			off = ( old_rw_min - cv->rw_min ) / new_rwc_delta_z;

			for ( sp = cv->points, count = cv->count; count > 0; sp++ )
				if ( sp->exist )
				{
					sp->v = fac * sp->v + off;
					count--;
				}

			if ( ! cv->is_fs )
			{
				cv->s2d[ Z ] /= fac;
				cv->shift[ Z ] = fac * cv->shift[ Z ] - off;
				cv->z_factor /= fac;
			}
		}

		/* If data have not been scaled yet to the interval [0,1] and maximum
		   value isn't identical to the minimum value anymore calculate the
		   scaling */

		if ( ! cv->is_scale_set && cv->rw_max != cv->rw_min )
		{
			fac = 1.0 / new_rwc_delta_z;
			off = - cv->rw_min / new_rwc_delta_z;

			for ( sp = cv->points, count = cv->count; count > 0; sp++ )
				if ( sp->exist )
				{
					sp->v = fac * sp->v + off;
					count--;
				}

			cv->is_scale_set = SET;

			if ( cv->active )
				scale_2d_changed[ X ] = scale_2d_changed[ Y ] = SET;
		}

		scale_2d_changed[ Z ] |= cv->active && cv->is_fs;
		need_cut_redraw |= cut_data_rescaled( curve, cv->rw_min, cv->rw_max );
		cv->rwc_delta[ Z ] = new_rwc_delta_z;
	}

	/* Now we're finished with rescaling and can set the new number of points
	   if necessary */

	if ( x_index + x_len > G2.nx )
		G2.nx = x_index + x_len;
	if ( y_index + y_len >= G2.ny )
		G2.ny = y_index + y_len + 1;

	/* Include the new data into the scaled data */

	if ( type & ( INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR ) )
	{
		for ( cur_ptr = ptr, i = x_index,
			  sp = cv->points + y_index * G2.nx + x_index;
			  i < x_index + x_len; sp++, i++ )
		{
			if ( type & ( INT_VAR | INT_ARR ) )
			{
				memcpy( &ldata, cur_ptr, sizeof ldata );
				data = ( double ) ldata;
				cur_ptr += sizeof ldata;
			}
			else
			{
				memcpy( &data, cur_ptr, sizeof data );
				cur_ptr += sizeof data;
			}

			if ( cv->is_scale_set )
				sp->v = ( data - cv->rw_min ) / cv->rwc_delta[ Z ];
			else
				sp->v = data;

			/* Increase the point count if the point is new and mark it as
			   set */

			if ( ! sp->exist )
			{
				cv->count++;
				sp->exist = SET;
			}
		}

		/* Tell the cross section handler about the new data, its return value
		   indicates if the cut needs to be redrawn */

		need_cut_redraw |= cut_new_points( curve, x_index, y_index, x_len );
	}
	else
	{
		ptr += sizeof y_len;
		for ( cur_ptr = ptr, i = y_index; i <= y_index + y_len; i++ )
		{
			sp = cv->points + i * G2.nx + x_index;
			memcpy( &x_len, cur_ptr, sizeof x_len );
			cur_ptr += sizeof x_len;

			for ( j = 0; j < x_len; sp++, j++ )
			{
				if ( type == INT_REF )
				{
					memcpy( &ldata, cur_ptr, sizeof ldata );
					data = ( double ) ldata;
					cur_ptr += sizeof ldata;
				}
				else
				{
					memcpy( &data, cur_ptr, sizeof data );
					cur_ptr += sizeof data;
				}

				if ( cv->is_scale_set )
					sp->v = ( data - cv->rw_min ) / cv->rwc_delta[ Z ];
				else
					sp->v = data;

				/* Increase the point count if the point is new and mark it as
				   set */

				if ( ! sp->exist )
				{
					cv->count++;
					sp->exist = SET;
				}
			}

			/* Tell the cross section handler about the new data, its return
			   value indicates if the cut needs to be redrawn */

			need_cut_redraw |= cut_new_points( curve, x_index, i, x_len );
		}
	}

	/* We're done when nothing is to be drawn because no scaling for the curve
	   is set */

	if ( ! cv->is_scale_set )
		return;

	/* Set flags that indicate what needs to be recalculated and what needs
	   to be redrawn */

	cv->rwc_start[ Z ] = cv->rw_min;

	/* Since new points were included a recalculation of the current curve
	   is required */

	cv->needs_recalc = SET;

	/* All other curves only need to be recalculated if the x- or y-size
	   has changed (and the curve is scaled at all) */

	for ( i = 0; i < G2.nc; i++ )
		if ( G2.curve_2d[ i ]->is_scale_set && i != curve )
			G2.curve_2d[ i ]->needs_recalc |= size_changed;

	/* We need a redraw of the canvas if the currently displayed curve
	   needs a recalculation because either the size(s) changed and ist's
	   in full scale mode or because new data points got added to the curve. */

	if ( G2.active_curve != -1 )
		need_2d_redraw |= G2.curve_2d[ G2.active_curve ]->needs_recalc &&
						  ( G2.curve_2d[ G2.active_curve ]->is_fs ||
							G2.curve_2d[ G2.active_curve ] == cv );
}


/*---------------------------------------------------------------------*/
/* Function used by all functions that unpack new data to be displayed */
/* to figure out how many new data there really are. When invoked *ptr */
/* points to the start of the new data set, and 'type' indicates the   */
/* type of the new data (integer or floating point numbers, 1D or      */
/* multi-dimensional array). It returns the number of of new data      */
/* points and '*ptr' is pointing to the start of the new data when the */
/* function returns.                                                   */
/*---------------------------------------------------------------------*/

static long get_number_of_new_points( char **ptr, int type )
{
	long len = 0;
	char *ptr_2d;
	long x_len;
	long y_len;
	long i;


	switch ( type )
	{
		case INT_VAR : case FLOAT_VAR :
			len = 1;
			break;

		case INT_ARR : case FLOAT_ARR :
			memcpy( &len, *ptr, sizeof len );
			*ptr += sizeof len;
			break;

		case INT_REF :
			memcpy( &y_len, *ptr, sizeof len );
			ptr_2d = *ptr + sizeof y_len;
			for ( i = 0; i < y_len; i++ )
			{
				memcpy( &x_len, ptr_2d, sizeof x_len );
				ptr_2d += sizeof x_len + x_len * sizeof( long );
				len = l_max( len, x_len );
			}
			break;

		case FLOAT_REF :
			memcpy( &y_len, *ptr, sizeof len );
			ptr_2d = *ptr + sizeof y_len;
			for ( i = 0; i < y_len; i++ )
			{
				memcpy( &x_len, ptr_2d, sizeof x_len );
				ptr_2d += sizeof x_len + x_len * sizeof( double );
				len = l_max( len, x_len );
			}
			break;

#ifndef NDEBUG
		default :
			eprint( FATAL, UNSET, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
#endif
	}

#ifndef NDEBUG
	if ( len <= 0 )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d, number of "
				"points to be drawn: %ld.\n", __FILE__, __LINE__, len );
		THROW( EXCEPTION );
	}
#endif

	return len;
}


/*----------------------------------------------------------*/
/* Determines the new maximum and minimum value of the data */
/* set and returns if the maximum or minimum changed.       */
/*----------------------------------------------------------*/

static bool get_new_extrema( double *max, double *min, char *ptr,
							 long len, int type )
{
	double data, old_max, old_min;
	long i, j;
	long ldata;
	long x_len;
	long y_len;


	old_max = *max;
	old_min = *min;

	if ( type & ( INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR ) )
		for ( i = 0; i < len; i++ )
		{
			if ( type & ( INT_VAR | INT_ARR ) )
			{
				memcpy( &ldata, ptr, sizeof ldata );
				data = ( double ) ldata;
				ptr += sizeof ldata;
			}
			else
			{
				memcpy( &data, ptr, sizeof data );
				ptr += sizeof data;
			}

			*max = d_max( data, *max );
			*min = d_min( data, *min );
		}
	else if ( type == INT_REF )
	{
		memcpy( &y_len, ptr, sizeof y_len );
		ptr += sizeof y_len;
		for ( i = 0; i < y_len; i++ )
		{
			memcpy( &x_len, ptr, sizeof x_len );
			ptr += sizeof x_len;
			for ( j = 0; j < x_len; j++ )
			{
				memcpy( &ldata, ptr, sizeof ldata );
				data = ( double ) ldata;
				ptr += sizeof ldata;
				*max = d_max( data, *max );
				*min = d_min( data, *min );
			}
		}
	}
	else if ( type == FLOAT_REF )
	{
		memcpy( &y_len, ptr, sizeof y_len );
		ptr += sizeof y_len;
		for ( i = 0; i < y_len; i++ )
		{
			memcpy( &x_len, ptr, sizeof x_len );
			ptr += sizeof x_len;
			for ( j = 0; j < x_len; j++ )
			{
				memcpy( &data, ptr, sizeof data );
				ptr += sizeof data;
				*max = d_max( data, *max );
				*min = d_min( data, *min );
			}
		}
	}

	return *max > old_max || *min < old_min;
}


/*-------------------------------------------------*/
/* Increments the number of 2D data in x-direction */
/*-------------------------------------------------*/

static bool incr_x( long x_index, long len )
{
	long i, j, k;
	Curve_2d *cv;
	long new_Gnx;
	long new_num;
	Scaled_Point *old_points;
	Scaled_Point *sp;


	new_Gnx = x_index + len;
	new_num = new_Gnx * G2.ny;

	for ( i = 0; i < G2.nc; i++ )
	{
		cv = G2.curve_2d[ i ];

		old_points = cv->points;
		sp = cv->points = SCALED_POINT_P T_malloc( new_num * sizeof *sp );

		for ( j = 0; j < G2.ny; j++ )
		{
			memcpy( sp, old_points + j * G2.nx, G2.nx * sizeof *sp );
			for ( k = G2.nx, sp += k; k < new_Gnx; sp++, k++ )
				sp->exist = UNSET;
		}

		T_free( old_points );

		cv->xpoints = XPOINT_P T_realloc( cv->xpoints,
										  new_num * sizeof *cv->xpoints );

		if ( cv->is_fs )
			cv->s2d[ X ] = ( double ) ( G2.canvas.w - 1 ) /
						   ( double ) ( new_Gnx - 1 );
	}

	/* Also tell the routines for drawing cuts about the changes and return
	   the return value of this function (which indicates if a redraw of the
	   cut is needed) */

	return cut_num_points_changed( X, new_Gnx );
}


/*-------------------------------------------------*/
/* Increments the number of 2D data in y-direction */
/*-------------------------------------------------*/

static bool incr_y( long y_index )
{
	long i, k;
	Curve_2d *cv;
	Scaled_Point *sp;
	long new_Gny;
	long new_num;


	new_Gny = y_index + 1;
	new_num = G2.nx * new_Gny;

	for ( i = 0; i < G2.nc; i++ )
	{
		cv = G2.curve_2d[ i ];

		cv->points = SCALED_POINT_P T_realloc( cv->points,
											   new_num * sizeof *cv->points );
		cv->xpoints = XPOINT_P T_realloc( cv->xpoints,
										  new_num * sizeof *cv->xpoints );

		for ( k = G2.ny * G2.nx, sp = cv->points + k; k < new_num; sp++, k++ )
			sp->exist = UNSET;

		if ( cv->is_fs )
			cv->s2d[ Y ] = ( double ) ( G2.canvas.h - 1 ) /
						   ( double ) ( new_Gny - 1 );
	}

	/* Also tell the routines for drawing cuts about the changes and return
	   the return value of this function (which indicates if a redraw of the
	   cut is needed) */

	return cut_num_points_changed( Y, new_Gny );
}


/*-------------------------------------------------------------*/
/* Increments the number of 2D data in both x- and y-direction */
/*-------------------------------------------------------------*/

static bool incr_x_and_y( long x_index, long len, long y_index )
{
	long i, j, k;
	Curve_2d *cv;
	long new_Gnx;
	long new_Gny;
	long new_num;
	Scaled_Point *old_points;
	Scaled_Point *sp;
	bool ret = UNSET;


	new_Gnx = x_index + len;
	new_Gny = y_index + 1;
	new_num = new_Gnx * new_Gny;

	for ( i = 0; i < G2.nc; i++ )
	{
		cv = G2.curve_2d[ i ];

		old_points = cv->points;
		sp = cv->points = SCALED_POINT_P T_malloc( new_num * sizeof *sp );

		/* Reorganise the old elements to fit into the new array and clear
		   the new elements in the already existing rows */

		for ( j = 0; j < G2.ny; j++ )
		{
			memcpy( sp, old_points + j * G2.nx, G2.nx * sizeof *sp );
			for ( k = G2.nx, sp += k; k < new_Gnx; sp++, k++ )
				sp->exist = UNSET;
		}

		/* Now also set the elements in the comletely new rows to unused */

		for ( j = new_Gnx * G2.ny; j < new_num; sp++, j++ )
			sp->exist = UNSET;

		T_free( old_points );

		cv->xpoints = XPOINT_P T_realloc( cv->xpoints,
										  new_num * sizeof *cv->xpoints );

		if ( cv->is_fs )
		{
			cv->s2d[ X ] = ( double ) ( G2.canvas.w - 1 ) /
						   ( double ) ( new_Gnx - 1 );
			cv->s2d[ Y ] = ( double ) ( G2.canvas.h - 1 ) /
						   ( double ) ( new_Gny - 1 );
		}
	}

	/* Also tell the routines for drawing cuts about the changes and return
	   the return value of this function (which indicates if a redraw of the
	   cut is needed) */

	ret = cut_num_points_changed( X, new_Gnx );
	return ret || cut_num_points_changed( Y, new_Gny );
}


/*
 * ---------------------------------------------------------------------------
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
