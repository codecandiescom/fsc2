/*
  $Id$
*/


#include "fsc2.h"


static void	unpack_and_accept( void *ptr );
static void	other_data_request( int type, void * ptr );
static void accept_1d_data( long x_index, long curve, int type, void *ptr );
static void accept_2d_data( long x_index, long y_index, long curve, int type,
							void *ptr );
static bool	incr_x( long x_index, long len );
static bool	incr_y( long y_index );
static bool	incr_x_and_y( long x_index, long len, long y_index );


/*---------------------------------------------------------------------------*/
/* This is the function that takes the new data from the message queue and   */
/* displays them. On a call of this function all data sets in the queue will */
/* be accepted, if there is a REQUEST in between (there can only be one      */
/* because REQUESTS need a reply) it will be moved to the end of the queue.  */
/*---------------------------------------------------------------------------*/

void accept_new_data( void )
{
	void *buf;
	int mq_next;
	int shm_id;

	
	while ( 1 )
	{
		/* Attach to the shared memory segment pointed to by the oldest
		   entry in the message queue */

		if ( ( buf = attach_shm( Message_Queue[ message_queue_low ].shm_id ) )
			 == ( void * ) - 1 )
		{
			eprint( FATAL, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
		}

		/* Unpack and accept the data sets (skip the length field) - if an
		   exception happens detach from segment and re-throw the exception */

		TRY
		{
			unpack_and_accept( buf + sizeof( long ) );
			TRY_SUCCESS;
		}
		OTHERWISE
		{
			detach_shm( buf, &Message_Queue[ message_queue_low ].shm_id );
			PASSTHROU( );
		}

		/* Detach from shared memory segment and remove it */

		detach_shm( buf, &Message_Queue[ message_queue_low ].shm_id );

		/* Increment the queue pointer */

		message_queue_low = ( message_queue_low + 1 ) % QUEUE_SIZE;

		/* Return if all entries in the message queue are used up */

		if ( message_queue_low == message_queue_high )
			break;

		/* Accept next data set if next entry in message queue is a data set */

		if ( Message_Queue[ message_queue_low ].type == DATA )
			continue;

		/* If the next entry is a REQUEST but the one following is a DATA set
		   exchange the entries and accept the data - this way the drawing of
		   all data is done before REQUESTs are honoured */

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

	if ( G.dim == 1 )
		redraw_all_1d( );
	else
		redraw_all_2d( );
}


/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/

static void unpack_and_accept( void *ptr )
{
	int i;
	int nsets;
	void *ptr_next;
	long x_index,
		 y_index,
		 curve;
	int type;
	long len;


	nsets = *( ( int * ) ptr );
	ptr += sizeof( int );

	if ( nsets < 0 )                /* special for clear curve commands etc. */
	{
		other_data_request( nsets, ptr );
		return;
	}

	/* Accept the new data from all data set */

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

			case INT_ARR :
				len = *( ( long * ) ptr );
				ptr_next = ptr + ( len + 1 ) * sizeof( long );
				break;

			case FLOAT_ARR :
				len = *( ( long * ) ptr );
				ptr_next = ptr + sizeof( long ) + len * sizeof( double );
				break;

			default :
				eprint( FATAL, "Internal communication error at %s:%d.\n",
						__FILE__, __LINE__ );
				THROW( EXCEPTION );
		}

		if ( G.dim == 1 )
			accept_1d_data( x_index, curve, type, ptr );
		else
			accept_2d_data( x_index, y_index, curve, type, ptr );
	}
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void other_data_request( int type, void * ptr )
{
	long i;
	long count;
	long *ca;
	int is_set;
	char *label[ 3 ];
	char lengths[ 3 ];


	switch( type )
	{
		case D_CLEAR_CURVE :                  /* clear curve command */
			count = * ( ( long * ) ptr );     /* length of list of curves */
			ptr += sizeof( long );
			ca = ( long * ) ptr;              /* list of curve numbers */

			for ( i = 0; i < count; i++ )
			{
				clear_curve( ca[ i ] );
				cut_clear_curve( ca[ i ] );
			}

			break;

		case D_CHANGE_SCALE :                 /* change scale command */
			is_set = *( ( int * ) ptr );      /* flags */
			ptr += sizeof( int );
			change_scale( is_set, ( double * ) ptr );
			break;

		case D_CHANGE_LABEL :                 /* rescale command */
			for ( i = X; i <= Z; i++ )
			{
				lengths[ i ] = * ( long * ) ptr;
				ptr += sizeof( long );
				label[ i ] = T_strdup( ( char * ) ptr );
				ptr += lengths[ i ];
			}
			change_label( label );
			break;

		case D_CHANGE_POINTS :                /* rescale command */
			rescale( * ( long * ) ptr, * ( ( long * ) ptr + 1 ) );
			break;

		default :                             /* unknown command */
			eprint( FATAL, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
	}
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void accept_1d_data( long x_index, long curve, int type, void *ptr )
{
	long len = 0;
	long *l_data;
	double *f_data;
	double rw_max,
		   rw_min;
	void *cur_ptr;
	double data;
	double new_rwc_delta_y;
	Curve_1d *cv;
	long i, j;
	Scaled_Point *sp;


	/* Test if the curve number is OK */

	if ( curve >= G.nc )
	{
		eprint( FATAL, "%s:%ld: There is no curve %ld.\n", Fname, Lc,
				curve + 1 );
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

		case INT_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			l_data = ( long * ) ptr;
			break;

		case FLOAT_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			f_data = ( double * ) ptr;
			break;

		default :
			eprint( FATAL, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	/* If the number of points exceeds the size of the arrays for the curves
	   we have to increase the sizes for all curves. But take care: While
	   x_index + len may be greater than G.nx, x_index can still be smaller
	   than G.nx ! */

	if ( x_index + len > G.nx )
	{
		for ( i = 0; i < G.nc; i++ )
		{
			cv = G.curve[ i ];

			cv->points = T_realloc( cv->points,
								  ( x_index + len ) * sizeof( Scaled_Point ) );
			cv->xpoints = T_realloc( cv->xpoints,
									 ( x_index + len ) * sizeof( XPoint ) );

			for ( j = G.nx, sp = cv->points + j; j < x_index + len; sp++, j++ )
				sp->exist = UNSET;

			if ( G.is_fs )
				cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) /
				                              ( double ) ( x_index + len - 1 );
		}

		if ( G.is_fs )
			G.scale_changed = SET;
	}

	/* Find maximum and minimum of old and new data */

	rw_max = G.rw_max;
	rw_min = G.rw_min;

	for ( cur_ptr = ptr, i = 0; i < len; i++ )
	{
		if ( type & ( INT_VAR | INT_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		rw_max = d_max( data, rw_max );
		rw_min = d_min( data, rw_min );
	}

	/* If the minimum or maximum has changed rescale all old scaled data */

	if ( G.rw_max < rw_max || G.rw_min > rw_min )
	{
		new_rwc_delta_y = rw_max - rw_min;

		if ( G.is_scale_set )
		{
			for ( i = 0; i < G.nc; i++ )
			{
				cv = G.curve[ i ];

				for ( j = 0, sp = cv->points; j < G.nx; sp++, j++ )
					if ( sp->exist )
						sp->v = ( G.rwc_delta[ Y ] * sp->v
								  + G.rw_min - rw_min ) / new_rwc_delta_y;

				if ( ! G.is_fs )
				{
					cv->s2d[ Y ] *= new_rwc_delta_y / G.rwc_delta[ Y ];
					cv->shift[ Y ] = ( G.rwc_delta[ Y ] * cv->shift[ Y ]
									   - G.rw_min + rw_min ) / new_rwc_delta_y;
				}
			}

			G.rwc_delta[ Y ] = new_rwc_delta_y;
		}

		/* If the data have not been scaled to [0,1] yet and the maximum
		   value isn't identical to the minimum do the scaling now */

		if ( ! G.is_scale_set && rw_max != rw_min )
		{
			for ( i = 0; i < G.nc; i++ )
				for ( j = 0, sp = G.curve[ i ]->points; j < G.nx; sp++, j++ )
					if ( sp->exist )
						  sp->v = ( sp->v - rw_min ) / new_rwc_delta_y;

			G.rwc_delta[ Y ] = new_rwc_delta_y;
			G.is_scale_set = SET;
		}

		G.rw_min = rw_min;
		G.rw_max = rw_max;
		G.scale_changed = SET;
	}

	/* Now we're finished with rescaling and can set the new number of points
	   if necessary */

	if ( x_index + len > G.nx )
		G.nx = x_index + len;

	/* Include the new data into the scaled data */

	for ( cur_ptr = ptr, i = x_index, sp = G.curve[ curve ]->points + x_index;
		  i < x_index + len; sp++, i++ )
	{
		if ( type & ( INT_VAR | INT_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = * ( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		if ( G.is_scale_set )
			sp->v = ( data - G.rw_min ) / G.rwc_delta[ Y ];
		else
			sp->v = data;

		/* Increase the point count if the point is new and mark it as set */

		if ( ! sp->exist )
		{
			G.curve[ curve ]->count++;
			sp->exist = SET;
		}
	}

	/* Calculate new points for display */

	if ( ! G.is_scale_set )
		return;

	G.rwc_start[ Y ] = rw_min;

	for ( i = 0; i < G.nc; i++ )
	{
		if ( ! G.scale_changed && i != curve )
			continue;
		recalc_XPoints_of_curve_1d( G.curve[ i ] );
	}

	G.scale_changed = UNSET;
}


/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

static void accept_2d_data( long x_index, long y_index, long curve, int type,
							void *ptr )
{
	long len = 0, count;
	long *l_data;
	double *f_data;
	double rw_max,
		   rw_min;
	void *cur_ptr;
	double data;
	double new_rwc_delta_z;
	Curve_2d *cv;
	long i, j;
	Scaled_Point *sp;
	bool need_cut_redraw = UNSET;
	bool xy_scale_changed = UNSET;


	/* Test if the curve number is OK */

	if ( curve >= G.nc )
	{
		eprint( FATAL, "%s:%ld: There is no curve %ld.\n", Fname, Lc,
				curve + 1 );
		THROW( EXCEPTION );
	}

	cv = G.curve_2d[ curve ];

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

		case INT_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			l_data = ( long * ) ptr;
			break;

		case FLOAT_ARR :
			len = *( ( long * ) ptr );
			ptr += sizeof( long );
			f_data = ( double * ) ptr;
			break;

		default :
			eprint( FATAL, "Internal communication error at %s:%d.\n",
					__FILE__, __LINE__ );
			THROW( EXCEPTION );
	}

	/* Now test if the new data fit into the already allocated memory,
	   otherwise extend the memory area. */

	if ( x_index + len > G.nx )
	{
		xy_scale_changed = SET;
		if ( y_index >= G.ny )
			need_cut_redraw |= incr_x_and_y( x_index, len, y_index );
		else
			need_cut_redraw |= incr_x( x_index, len );
	}
	else if ( y_index >= G.ny )
	{
		xy_scale_changed = SET;
		need_cut_redraw |= incr_y( y_index );
	}

	/* Find maximum and minimum of old and new data */

	rw_max = cv->rw_max;
	rw_min = cv->rw_min;

	for ( cur_ptr = ptr, i = 0; i < len; i++ )
	{
		if ( type & ( INT_VAR | INT_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
		}

		rw_max = d_max( data, rw_max );
		rw_min = d_min( data, rw_min );
	}

	/* If minimum or maximum changed all old scaled data have to be rescaled */

	if ( cv->rw_max < rw_max || cv->rw_min > rw_min )
	{
		new_rwc_delta_z = rw_max - rw_min;

		if ( cv->is_scale_set )
		{
			for ( sp = cv->points, count = cv->count, j = 0;
				  count != 0 && j < G.nx * G.ny; sp++, j++ )
				if ( sp->exist )
				{
					sp->v = ( cv->rwc_delta[ Z ] * sp->v + cv->rw_min
							  - rw_min ) / new_rwc_delta_z;
					count--;
				}

			if ( ! cv->is_fs )
			{
				cv->s2d[ Z ] *= new_rwc_delta_z / cv->rwc_delta[ Z ];
				cv->shift[ Z ] = ( cv->rwc_delta[ Z ] * cv->shift[ Z ]
								   - cv->rw_min + rw_min ) / new_rwc_delta_z;
				cv->z_factor *= new_rwc_delta_z / cv->rwc_delta[ Z ];
			}
		}

		/* If the data have not been scaled to [0,1] yet and the maximum
		   value isn't identical to the minimum do the scaling now */

		if ( ! cv->is_scale_set && rw_max != rw_min )
		{
			for ( sp = cv->points, count = cv->count, j = 0;
				  count != 0 && j < G.nx * G.ny; sp++, j++ )
				if ( sp->exist )
				{
					sp->v = ( sp->v - rw_min ) / new_rwc_delta_z;
					count--;
				}

			cv->is_scale_set = SET;
		}

		need_cut_redraw |= cut_data_rescaled( curve, rw_min, rw_max );

		cv->rwc_delta[ Z ] = new_rwc_delta_z;

		cv->rw_min = rw_min;
		cv->rw_max = rw_max;
		cv->scale_changed = SET;

	}

	/* Now we're finished with rescaling and can set the new number of points
	   if necessary */

	if ( x_index + len > G.nx )
		G.nx = x_index + len;
	if ( y_index >= G.ny )
		G.ny = y_index + 1;

	/* Include the new data into the scaled data */

	for ( cur_ptr = ptr, i = x_index,
		  sp = cv->points + y_index * G.nx + x_index;
		  i < x_index + len; sp++, i++ )
	{
		if ( type & ( INT_VAR | INT_ARR ) )
		{
			data = ( double ) *( ( long * ) cur_ptr );
			cur_ptr += sizeof( long );
		}
		else
		{
			data = *( ( double * ) cur_ptr );
			cur_ptr += sizeof( double );
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

	/* Tell the cross section handler about the new data */

	need_cut_redraw |= cut_new_points( curve, x_index, y_index, len );

	if ( ! cv->is_scale_set )
		return;

	/* Calculate new points for display */

	cv->rwc_start[ Z ] = rw_min;

	if ( cv->is_scale_set )
	recalc_XPoints_of_curve_2d( cv );

	for ( i = 0; i < G.nc; i++ )
	{
		if ( i == curve )
			continue;
		cv = G.curve_2d[ i ];
		if ( xy_scale_changed && cv->is_scale_set )
			 recalc_XPoints_of_curve_2d( cv );
	}

	/* Update the cut window if necessary */

	if ( need_cut_redraw )
		redraw_all_cut_canvases( );

	for ( i = 0; i < G.nc; i++ )
		cv->scale_changed = UNSET;
}


/*----------------------------------------------*/
/* Increments the number of data in x-direction */
/*----------------------------------------------*/

static bool incr_x( long x_index, long len )
{
	long i, j, k;
	Curve_2d *cv;
	long new_Gnx = x_index + len;
	Scaled_Point *old_points;
	Scaled_Point *sp;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve_2d[ i ];
			
		old_points = cv->points;
		cv->points = T_malloc( new_Gnx * G.ny * sizeof( Scaled_Point ) );

		for ( sp = cv->points, j = 0; j < G.ny; j++ )
		{
			memcpy( sp, old_points + j * G.nx, G.nx * sizeof( Scaled_Point ) );
			for ( sp += G.nx, k = G.nx; k < new_Gnx; sp++, k++ )
				sp->exist = UNSET;
		}

		T_free( old_points );

		cv->xpoints = T_realloc( cv->xpoints,
								 new_Gnx * G.ny * sizeof( XPoint ) );
		cv->xpoints_s = T_realloc( cv->xpoints_s,
								   new_Gnx * G.ny * sizeof( XPoint ) );

		if ( cv->is_fs )
			cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) /
			                                        ( double ) ( new_Gnx - 1 );
		cv->scale_changed = SET;
	}

	return cut_num_points_changed( X, new_Gnx );
}


/*----------------------------------------------*/
/* Increments the number of data in y-direction */
/*----------------------------------------------*/

static bool incr_y( long y_index )
{
	long i, j, k;
	Curve_2d *cv;
	Scaled_Point *sp;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve_2d[ i ];
			
		cv->points    = T_realloc( cv->points, G.nx * ( y_index + 1 )
								   * sizeof( Scaled_Point ) );
		cv->xpoints   = T_realloc( cv->xpoints, G.nx * ( y_index + 1 )
								   * sizeof( XPoint ) );
		cv->xpoints_s = T_realloc( cv->xpoints_s, G.nx * ( y_index + 1 )
								   * sizeof( XPoint ) );

		for ( sp = cv->points +  G.ny * G.nx, j = G.ny; j <= y_index; j++ )
			for ( k = 0; k < G.nx; sp++, k++ )
				sp->exist = UNSET;

		if ( cv->is_fs )
			cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / ( double ) y_index;
		cv->scale_changed = SET;
	}

	return cut_num_points_changed( Y, y_index + 1 );
}


/*----------------------------------------------------------*/
/* Increments the number of data in both x- and y-direction */
/*----------------------------------------------------------*/

static bool incr_x_and_y( long x_index, long len, long y_index )
{
	long i, j, k;
	Curve_2d *cv;
	long new_Gnx = x_index + len;
	Scaled_Point *old_points;
	Scaled_Point *sp;
	bool ret = UNSET;


	for ( i = 0; i < G.nc; i++ )
	{
		cv = G.curve_2d[ i ];
			
		old_points = cv->points;
		cv->points = T_malloc( new_Gnx * ( y_index + 1 )
							   * sizeof( Scaled_Point ) );

		/* Reorganise the old elements to fit into the new array and clear
		   the the new elements in the already existing rows */

		for ( sp = cv->points, j = 0; j < G.ny; j++ )
		{
			memcpy( sp, old_points + j * G.nx, G.nx * sizeof( Scaled_Point ) );
			for ( sp += G.nx, k = G.nx; k < new_Gnx; sp++, k++ )
				sp->exist = UNSET;
		}

		for ( ; j <= y_index; j++ )
			for ( k = 0; k < new_Gnx; sp++, k++ )
				sp->exist = UNSET;

		T_free( old_points );

		cv->xpoints = T_realloc( cv->xpoints, new_Gnx * ( y_index + 1 )
								              * sizeof( XPoint ) );
		cv->xpoints_s = T_realloc( cv->xpoints_s, new_Gnx * ( y_index + 1 )
								                  * sizeof( XPoint ) );

		if ( cv->is_fs )
		{
			cv->s2d[ X ] = ( double ) ( G.canvas.w - 1 ) /
			                                        ( double ) ( new_Gnx - 1 );
			cv->s2d[ Y ] = ( double ) ( G.canvas.h - 1 ) / ( double ) y_index;
		}

		cv->scale_changed = SET;
	}

	ret = cut_num_points_changed( X, new_Gnx );
	return ret || cut_num_points_changed( Y, y_index + 1 );
}
