/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

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


/*---------------------------------------------------------------------------*/
/* This is the function that takes the new data from the message queue and   */
/* displays them. On a call of this function all data sets in the queue will */
/* be accepted, if there is a REQUEST in between (there can only be one at a */
/* time because REQUESTS need a reply) it will be moved to the end of the    */
/* queue and then are dealt with by the the calling routine.                 */
/*---------------------------------------------------------------------------*/

void accept_new_data( void )
{
	char *buf;
	int mq_next;
	int type;
	static int dim;


	dim = 0;

	while ( 1 )
	{
		/* Attach to the shared memory segment pointed to by the oldest
		   entry in the message queue - even though this should never
		   fail it sometimes does (2.0 kernels only?) so we better have
		   a bit more of error output until this is sorted out. */

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

		/* Unpack and accept the data sets (skip the length field) - if an
		   exception happens detach from segment and re-throw the exception */

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
		sema_post( Comm.data_semaphore );

		/* Return if all entries in the message queue are used up */

		if ( Comm.MQ->low == Comm.MQ->high )
			break;

		/* Accept next data set if next slot in message queue contains DATA */

		if ( Comm.MQ->slot[ Comm.MQ->low ].type == DATA_1D ||
			 Comm.MQ->slot[ Comm.MQ->low ].type == DATA_2D )
			continue;

		/* Otherwise the next slot is occupied by a REQUEST. If this is the
		   last occupied slot we're done (the calling routine will deal with
		   it). Otherwise there are more DATA that we handle first by
		   exchanging them with the current REQUEST entry. */

  		if ( ( mq_next = ( Comm.MQ->low + 1 ) % QUEUE_SIZE ) == Comm.MQ->high )
  			break;

  		Comm.MQ->slot[ Comm.MQ->low ].shm_id = Comm.MQ->slot[ mq_next ].shm_id;
  		Comm.MQ->slot[ Comm.MQ->low ].type = Comm.MQ->slot[ mq_next ].type;

  		Comm.MQ->slot[ mq_next ].shm_id = -1;
  		Comm.MQ->slot[ mq_next ].type = REQUEST;
  	}

	/* Finally display the new data by redrawing the canvas */

	if ( dim & 1 )
		redraw_all_1d( );
	if ( dim & 2 )
		redraw_all_2d( );
}


/*------------------------------------------------------------------*/
/* This function examines the new data interpreting the start field */
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
	   sets that needs handling. The only exeption is when this indicator
	   is negative, in which case these aren't data to be displayed but
	   special commands for clearing curves, scale changes etc. */

	memcpy( &nsets, ptr, sizeof nsets );
	ptr += sizeof nsets;

	if ( nsets < 0 )                /* special for clear curve commands etc. */
	{
		other_data_request( dim, nsets, ptr );
		return;
	}

	/* Accept the new data from all data set */

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
				if ( len == 0 )
					continue;
				ptr_next = ptr + sizeof len + len * sizeof( long );
				break;

			case FLOAT_ARR :
				memcpy( &len, ptr, sizeof len );
				if ( len == 0 )
					continue;
				ptr_next = ptr + sizeof len + len * sizeof( double );
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


/*--------------------------------------------------------------------------*/
/* Function for handling special commands that come disguised as data, like */
/* commands for clearing curves, changing scales etc. Here the type of the  */
/* command is determined and the appropriate functions are called.          */
/*--------------------------------------------------------------------------*/

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
					cut_clear_curve( ca );
				}
			}

			break;

		case D_CHANGE_SCALE :                       /* change scale command */
			memcpy( &is_set, ptr, sizeof is_set );  /* flags */
			ptr += sizeof is_set;
			if ( dim == DATA_1D )
				change_scale_1d( is_set, ( void * ) ptr );
			else
				change_scale_2d( is_set, ( void * ) ptr );
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
				change_label_1d( label );
			else
				change_label_2d( label );
			break;

		case D_CHANGE_POINTS :                /* rescale command */
			if ( dim == DATA_1D )
				rescale_1d( * ( long * ) ptr );
			else
				rescale_2d( ( void * ) ptr );
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
			}
			break;

		case D_CLEAR_MARKERS :
			if ( dim == DATA_1D )
				remove_markers_1d( );
			else
				remove_markers_2d( ( long * ) ptr );
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


/*---------------------------------------------------------------------*/
/* This function handles the storing and displaying of new data points */
/* when 1D graphics is used.                                           */
/*---------------------------------------------------------------------*/

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

		G1.scale_changed = G1.is_fs;
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
		}

		G1.scale_changed = SET;
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

	if ( ! G1.scale_changed )
		recalc_XPoints_of_curve_1d( G1.curve[ curve ] );
	else
		for ( i = 0; i < G1.nc; i++ )
			recalc_XPoints_of_curve_1d( G1.curve[ i ] );

	G1.scale_changed = UNSET;
}



/*---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/

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

			G1.is_scale_set = SET;
		}

		G1.scale_changed = SET;
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
				m = MARKER_1D_P T_free( m );
				m = mn;
			}
			else
			{
				mp = m;
				m = m->next;
			}
		}

		G1.scale_changed = SET;
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

	/* If the scale did not change redraw only the current curve, otherwise all
	   curves */

	if ( ! G1.scale_changed )
		recalc_XPoints_of_curve_1d( G1.curve[ curve ] );
	else
		for ( i = 0; i < G1.nc; i++ )
			recalc_XPoints_of_curve_1d( G1.curve[ i ] );

	G1.scale_changed = UNSET;
}



/*---------------------------------------------------------------------*/
/* This function handles the storing and displaying of new data points */
/* when 2D graphics is used.                                           */
/*---------------------------------------------------------------------*/

static void accept_2d_data( long x_index, long y_index, long curve, int type,
							char *ptr )
{
	long len = 0, count;
	char *cur_ptr;
	double data;
	long ldata;
	double new_rwc_delta_z, fac, off, old_rw_min;
	Curve_2d *cv;
	long i;
	Scaled_Point *sp;
	bool need_cut_redraw = UNSET;
	bool xy_scale_changed = UNSET;


	/* Test if the curve number is OK */

#ifndef NDEBUG
	if ( curve >= G2.nc )
	{
		eprint( FATAL, UNSET, "Internal error detected at %s:%d, there is no "
				"curve %ld.\n", __FILE__, __LINE__, curve + 1 );
		THROW( EXCEPTION );
	}
#endif

	/* Get number of new data points and pointer to start of data */

	len = get_number_of_new_points( &ptr, type );

	cv = G2.curve_2d[ curve ];

	/* Check if the new data fit into the already allocated memory, otherwise
	   extend the memory area. */

	if ( x_index + len > G2.nx )
	{
		xy_scale_changed = SET;
		if ( y_index >= G2.ny )
			need_cut_redraw |= incr_x_and_y( x_index, len, y_index );
		else
			need_cut_redraw |= incr_x( x_index, len );
	}
	else if ( y_index >= G2.ny )
	{
		xy_scale_changed = SET;
		need_cut_redraw |= incr_y( y_index );
	}

	/* Find maximum and minimum of old and new data and, if the minimum or
	   maximum changed, all old data have to be (re)scaled */

	old_rw_min = cv->rw_min;

	if ( ( cv->scale_changed =
		   get_new_extrema( &cv->rw_max, &cv->rw_min, ptr, len, type ) ) )
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

		/* If data have not been scaled yet to interval [0,1] and maximum
		   value has become different from minimum value do scaling now */

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
		}

		need_cut_redraw |= cut_data_rescaled( curve, cv->rw_min, cv->rw_max );
		cv->rwc_delta[ Z ] = new_rwc_delta_z;
	}

	/* Now we're finished with rescaling and can set the new number of points
	   if necessary */

	if ( x_index + len > G2.nx )
		G2.nx = x_index + len;
	if ( y_index >= G2.ny )
	{
		G2.ny = y_index + 1;
		need_cut_redraw = SET;
	}

	/* Include the new data into the scaled data */

	for ( cur_ptr = ptr, i = x_index,
		  sp = cv->points + y_index * G2.nx + x_index;
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

		if ( cv->is_scale_set )
			sp->v = ( data - cv->rw_min ) / cv->rwc_delta[ Z ];
		else
			sp->v = data;

		/* Increase the point count if the point is new and mark it as set */

		if ( ! sp->exist )
		{
			cv->count++;
			sp->exist = SET;
		}
	}

	/* Tell the cross section handler about the new data, its return value
	   indicates if the cut needs to be redrawn */

	need_cut_redraw |= cut_new_points( curve, x_index, y_index, len );

	/* We're done when nothing is to be drawn because no scaling for the curve
	   is set */

	if ( ! cv->is_scale_set )
		return;

	/* Calculate new points for display */

	cv->rwc_start[ Z ] = cv->rw_min;

	for ( i = 0; i < G2.nc; i++ )
	{
		cv = G2.curve_2d[ i ];

		if ( ! cv->is_scale_set )
			continue;

		if ( xy_scale_changed || i == curve )
			 recalc_XPoints_of_curve_2d( cv );
		cv->scale_changed = UNSET;
	}

	/* Finally update the cut window if necessary */

	if ( need_cut_redraw )
		redraw_all_cut_canvases( );
}


/*----------------------------------------------------------*/
/*----------------------------------------------------------*/

static long get_number_of_new_points( char **ptr, int type )
{
	long len = 0;


	switch ( type )
	{
		case INT_VAR : case FLOAT_VAR :
			len = 1;
			break;

		case INT_ARR : case FLOAT_ARR :
			memcpy( &len, *ptr, sizeof len );
			*ptr += sizeof len;
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
/* set and returns if the maximum or minimum changes.       */
/*----------------------------------------------------------*/

static bool get_new_extrema( double *max, double *min, char *ptr,
							 long len, int type )
{
	double data, old_max, old_min;
	long i;
	long ldata;


	old_max = *max;
	old_min = *min;

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

	return *max > old_max || *min < old_min;
}


/*----------------------------------------------*/
/* Increments the number of data in x-direction */
/*----------------------------------------------*/

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
		cv->xpoints_s = XPOINT_P T_realloc( cv->xpoints_s,
											new_num * sizeof *cv->xpoints_s );

		if ( cv->is_fs )
			cv->s2d[ X ] = ( double ) ( G2.canvas.w - 1 ) /
						   ( double ) ( new_Gnx - 1 );
		cv->scale_changed = SET;
	}

	/* Also tell the routines for drawing cuts about the changes and return
	   the return value of this function (which indicates if a redraw of the
	   cut is needed) */

	return cut_num_points_changed( X, new_Gnx );
}


/*----------------------------------------------*/
/* Increments the number of data in y-direction */
/*----------------------------------------------*/

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
		cv->xpoints_s = XPOINT_P T_realloc( cv->xpoints_s,
											new_num * sizeof *cv->xpoints_s );

		for ( k = G2.ny * G2.nx, sp = cv->points + k; k < new_num; sp++, k++ )
			sp->exist = UNSET;

		if ( cv->is_fs )
			cv->s2d[ Y ] = ( double ) ( G2.canvas.h - 1 ) /
						   ( double ) ( new_Gny - 1 );
		cv->scale_changed = SET;
	}

	/* Also tell the routines for drawing cuts about the changes and return
	   the return value of this function (which indicates if a redraw of the
	   cut is needed) */

	return cut_num_points_changed( Y, new_Gny );
}


/*----------------------------------------------------------*/
/* Increments the number of data in both x- and y-direction */
/*----------------------------------------------------------*/

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
		cv->xpoints_s = XPOINT_P T_realloc( cv->xpoints_s,
											new_num * sizeof *cv->xpoints_s );

		if ( cv->is_fs )
		{
			cv->s2d[ X ] = ( double ) ( G2.canvas.w - 1 ) /
						   ( double ) ( new_Gnx - 1 );
			cv->s2d[ Y ] = ( double ) ( G2.canvas.h - 1 ) /
						   ( double ) ( new_Gny - 1 );
		}

		cv->scale_changed = SET;
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
