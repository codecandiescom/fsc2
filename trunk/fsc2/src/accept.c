/*
  $Id$
*/


#include "fsc2.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/param.h>



static void accept_1d_data( long x_index, long curve, int type, void *ptr );
static void accept_2d_data( long x_index, long y_index, long curve, int type,
							void *ptr );


/*---------------------------------------------------------------------------*/
/* This is the function that takes the new data from the message queue and   */
/* displays them. On a call of this function all data sets in the queue will */
/* be accepted, if there is a REQUEST in between it will be moved to the end */
/* of the queue.                                                             */
/*---------------------------------------------------------------------------*/

void accept_new_data( void )
{
	void *buf,
		 *ptr,
		 *ptr_next;
	int i;
	int nsets;
	long x_index,
		 y_index,
		 curve;
	int type;
	long len;
	int mq_next;
	int shm_id;


	while ( 1 )
	{
		/* Attach to the shared memory segment */

		if ( ( buf = shmat( Message_Queue[ message_queue_low ].shm_id,
							NULL, SHM_RDONLY ) ) == ( void * ) - 1 )
		{
			shmctl( Message_Queue[ message_queue_low ].shm_id,
					IPC_RMID, NULL );                  /* delete the segment */
			eprint( FATAL, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		/* Skip the magic number at the start and the total length field */

		ptr = buf + 4 * sizeof( char ) + sizeof( long );

		/* Get the number of data sets */

		nsets = *( ( int * ) ptr);
		ptr += sizeof( int );

		/* Accept the new data from each data set */

		for ( i = 0; i < nsets; ptr = ptr_next, i++ )
		{
			x_index = *( ( long * ) ptr );
			ptr += sizeof( long );

			y_index = *( ( long * ) ptr );
			ptr += sizeof( long );

			curve = *( ( long * ) ptr );
			ptr += sizeof( long );

			type = *( ( int * ) ptr );
			ptr += sizeof( int );
			
			switch ( type )
			{
				case INT_VAR :
					ptr_next = ptr + sizeof( long );
					break;

				case FLOAT_VAR :
					ptr_next = ptr + sizeof( double );
					break;

				case INT_TRANS_ARR :
					len = *( ( long * ) ptr );
					ptr_next = ptr + ( len + 1 ) * sizeof( long );
					break;

				case FLOAT_TRANS_ARR :
					len = *( ( long * ) ptr );
					ptr_next = ptr + sizeof( long ) + len * sizeof( double );
					break;
			}

			TRY
			{
				if ( G.dim == 1 )
					accept_1d_data( x_index, curve, type, ptr );
				else
					accept_2d_data( x_index, y_index, curve, type, ptr );
				TRY_SUCCESS;
			}
			CATCH( EXCEPTION )
			{
				shmdt( ( void * ) buf );
				shmctl( Message_Queue[ message_queue_low ].shm_id,
						IPC_RMID, NULL );
				THROW( EXCEPTION );
			}
		}

		/* Finally detach from shared memory segment and remove it */

		shmdt( ( void * ) buf );
		shmctl( Message_Queue[ message_queue_low ].shm_id, IPC_RMID, NULL );

		/* Increment the queue pointer */

		message_queue_low = ( message_queue_low + 1 ) % QUEUE_SIZE;

		/* Return if all entries in the message queue are used up */

		if ( message_queue_low == message_queue_high )
			break;

		/* Accept next data set if next entry in message queue is a data set */

		if ( Message_Queue[ message_queue_low ].type == DATA )
			continue;

		/* If the new entry is a REQUEST but the following a DATA set exchange
		   the entries and accept the data - this way the drawing of all data
		   is done before REQUESTs are honored */

		mq_next = ( message_queue_low + 1 ) % QUEUE_SIZE;

		if ( mq_next == message_queue_high )
			break;                                  /* REQUEST is last entry */

		/* Swap current REQUEST with next DATA set */

		shm_id = Message_Queue[ mq_next ].shm_id;

		Message_Queue[ mq_next ].shm_id =
			                         Message_Queue[ message_queue_low ].shm_id;
		Message_Queue[ mq_next ].type = REQUEST;

		Message_Queue[ message_queue_low ].shm_id = shm_id;
		Message_Queue[ message_queue_low ].type = DATA;
	}

	/* Finally display the new data by redrawing the canvas */

	redraw_canvas( &G.canvas );
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void accept_1d_data( long x_index, long curve, int type, void *ptr )
{
	long len;
	long *l_data;
	double *f_data;

	double rw_y_max,
		   rw_y_min;
	void *cur_ptr;
	double data;
	double new_y_scale;
	Curve_1d *cv;
	long i, j;


	/* Check that the curve number is ok */

	if ( curve >= G.nc )
	{
		eprint( FATAL, "$s:%ld: Curve %ld not declared.\n", Fname, Lc,
				curve + ARRAY_OFFSET );
		THROW( EXCEPTION );
	}

	/* Get the amount of new data and a pointer to the start of the data */

	switch ( type )
	{
		case INT_VAR :
			len = 1;
			l_data = ( long * ) ptr;
			break;

		case FLOAT_VAR :
			len = 1;
			f_data = ( double * ) ptr;
			break;

		case INT_TRANS_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			l_data = ( long * ) ptr;
			break;

		case FLOAT_TRANS_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			f_data = ( double * ) ptr;
			break;
	}

	/* If the number of points exceeds the size of the arrays for the curves
	   we print an error message if the number was fixed by the call to
	   init_1d(), otherwise we have to increase the sizes for all curves */

	if ( x_index + len > G.nx )
	{
		if ( G.is_nx )       /* number of points has been fixed by init_1d() */
		{
			eprint( WARN, "%s:%ld: More points than declared in "
					"`init_1d()'.\n", Fname, Lc );
			G.is_nx = UNSET;
		}

		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			cv->points = T_realloc( cv->points,
								  ( x_index + len ) * sizeof( Scaled_Point ) );
			cv->xpoints = T_realloc( cv->xpoints,
										( x_index + len ) * sizeof( XPoint ) );
			for ( j = x_index; j < x_index + len; j++ )
				cv->points[ j ].exist = UNSET;
			cv->s2d_x = ( double ) ( G.canvas.w - 1 ) /
				                                       ( double ) ( G.nx - 1 );
		}

		G.scale_changed = SET;
	}

	/* Find maximum and minimum of old and new data */

	rw_y_max = G.rw_y_max;
	rw_y_min = G.rw_y_min;

	for ( cur_ptr = ptr, i = 0; i < len; i++ )
	{
		if ( type & ( INT_VAR | INT_TRANS_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		rw_y_max = d_max( data, rw_y_max );
		rw_y_min = d_min( data, rw_y_min );
	}

	/* If the minimum or maximum changed rescale all old scaled data */

	if ( G.rw_y_max < rw_y_max || G.rw_y_min > rw_y_min )
	{
		if ( G.is_scale_set )
		{
			new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				for ( j = 0; j < G.nx; j++ )
				{
					if ( cv->points[ j ].exist )
						cv->points[ j ].y = new_y_scale
							* ( cv->points[ j ].y / G.rw2s
								+ G.rw_y_min - rw_y_min );
				}

				if ( ! G.is_fs )
				{
					cv->s2d_y *= G.rw2s / new_y_scale;
					cv->y_shift *= new_y_scale / G.rw2s;
					if ( rw_y_max > G.rw_y_max )
						cv->y_shift += ( rw_y_max - G.rw_y_max ) * new_y_scale;
				}
			}

			G.rw2s = new_y_scale;
		}

		/* If the data have not been scaled to [0,1] yet and the maximum
		   value isn't identical to the minimum do the scaling now */

		if ( ! G.is_scale_set && rw_y_max != rw_y_min )
		{
			new_y_scale = 1.0 / ( rw_y_max - rw_y_min );

			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];
				for ( j = 0; j < G.nx; j++ )
					if ( cv->points[ j ].exist )
						cv->points[ j ].y = new_y_scale
							                * ( cv->points[ j ].y - rw_y_min );
			}

			G.rw2s = new_y_scale;
			G.is_scale_set = SET;
		}

		G.rw_y_min = rw_y_min;
		G.rw_y_max = rw_y_max;
		G.scale_changed = SET;
	}

	/* Include the new data into the scaled data */

	for ( cur_ptr = ptr, i = x_index; i < x_index + len; i++ )
	{
		if ( type & ( INT_VAR | INT_TRANS_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		if ( G.is_scale_set )
			G.curve[ curve ]->points[ i ].y = G.rw2s * ( data - G.rw_y_min );
		else
			G.curve[ curve ]->points[ i ].y = data;

		/* Increase the point count if the point is new and mark it as set */

		if ( ! G.curve[ curve ]->points[ i ].exist )
		{
			G.curve[ curve ]->count++;
			G.curve[ curve ]->points[ i ].exist = SET;
		}
	}

	/* Calculate new points for display */

	if ( ! G.is_scale_set )
		return;

	for ( i = 0; i < G.nc; i++ )
	{
		if ( ! G.scale_changed && i != curve )
			continue;
		recalc_XPoints_of_curve( G.curve[ i ] );
	}

	G.scale_changed = UNSET;
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void accept_2d_data( long x_index, long y_index, long curve, int type,
					 void *ptr )
{
}
