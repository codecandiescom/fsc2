/*
 *  Copyright (C) 1999-2015 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "fsc2.h"


static void unpack_and_accept( int          dim,
                               const char * ptr );
static void other_data_request( int          dim,
                                int          type,
                                const char * ptr );
static void accept_1d_data( long         x_index,
                            long         curve,
                            Var_Type_T   type,
                            const char * ptr );
static void accept_1d_data_sliding( long         curve,
                                    Var_Type_T   type,
                                    const char * ptr );
static void accept_2d_data( long         x_index,
                            long         y_index,
                            long         curve,
                            Var_Type_T   type,
                            const char * ptr );
static long get_number_of_new_points( const char ** ptr,
                                      Var_Type_T    type );
static bool get_new_extrema( double     * max,
                             double     * min,
                             const char * ptr,
                             long         len,
                             Var_Type_T   type );
static bool incr_x( long x_index,
                    long len );
static bool incr_y( long y_index );
static bool incr_x_and_y( long x_index,
                          long len,
                          long y_index );


/* These variables are used to determine which parts of the display(s) need
   redrawing after the data have been unpacked in order to minimize the
   number of graphical operations */

static bool Scale_1d_changed[ 2 ];
static bool Scale_2d_changed[ 3 ];
static bool Need_2d_redraw;
static bool Need_cut_redraw;


#define MAX_ACCEPT_TIME  0.2    /* 200 ms */


/*--------------------------------------------------------------------------*
 * This is the function that takes new data from the message queue and
 * displays them. The function is invoked as an idle callback, i.e. whenever
 * the program has nothing else to do for a limited time (currently set to
 * 200 ms), and schedules them to be displayed. When all sets have been
 * removed from the message queue (or the maximum time in the accept loop is
 * over or only a REQUEST type date item is left, which is always the last
 * if one exists) all parts of the display windows that changed due to the
 * new data get updated.
 *--------------------------------------------------------------------------*/

void
accept_new_data( bool empty_queue )
{
    /* Get the time we arrived here, it's later used to avoid spending too
       much time in the loop for accepting data sets */

    volatile double start_time = 0.0;
    if ( ! empty_queue )
    {
        struct timeval time_struct;
        gettimeofday( &time_struct, NULL );
        start_time = time_struct.tv_sec + 1.0e-6 * time_struct.tv_sec;
    }

    /* Clear the flags that later tell us what really needs to be redrawn */

    memset( Scale_1d_changed, 0, sizeof Scale_1d_changed );
    memset( Scale_2d_changed, 0, sizeof Scale_2d_changed );
    Need_2d_redraw = Need_cut_redraw = false;

    volatile int dim = 0;

    while ( true )
    {
        /* Attach to the shared memory segment pointed to by the oldest entry
           in the message queue - even though this should never fail it
           sometimes does (with 2.0 kernels only) */

        const char * volatile buf;
        if ( ! ( buf = attach_shm( Comm.MQ->slot[ Comm.MQ->low ].shm_id ) ) )
        {
#ifndef NDEBUG
            eprint( FATAL, false, "Internal communication error at %s:%d, "
                    "message_queue_low = %d, shm_id = %d.\n"
                    "*** PLEASE SEND A BUG REPORT CITING THESE LINES *** "
                    "Thank you.\n",
                    __FILE__, __LINE__, Comm.MQ->low,
                    Comm.MQ->slot[ Comm.MQ->low ].shm_id );
#else
            eprint( FATAL, false, "Internal communication error at %s:%d.\n",
                    __FILE__, __LINE__ );
#endif
            THROW( EXCEPTION );
        }

        /* Unpack and accept the data sets (skip the length field, it's not
           needed) - if an exception happens detach from the segment and
           re-throw the exception */

        TRY
        {
            int type = Comm.MQ->slot[ Comm.MQ->low ].type;
            dim |= type == DATA_1D ? 1 : 2;
            unpack_and_accept( type, buf + sizeof( long ) );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            detach_shm( buf, &Comm.MQ->slot[ Comm.MQ->low ].shm_id );
            RETHROW;
        }

        /* Detach from the shared memory segment and remove it */

        detach_shm( buf, &Comm.MQ->slot[ Comm.MQ->low ].shm_id );
        Comm.MQ->slot[ Comm.MQ->low ].shm_id = -1;

        /* Increment the low queue pointer and post the data semaphore to
           tell the child that there's again room in the message queue. */

        Comm.MQ->low = ( Comm.MQ->low + 1 ) % QUEUE_SIZE;
        sema_post( Comm.mq_semaphore );

        /* Leave the loop if all entries in the message queue are used up or
           there's only a REQUEST left (which is always the last entry because
           the child has to wait for a reply and which gets handled by the
           calling routine) */

        if (    Comm.MQ->low == Comm.MQ->high
             || Comm.MQ->slot[ Comm.MQ->low ].type == REQUEST )
            break;

        /* Also break from the loop after about MAX_ACCEPT_TIME seconds unless
           'empty_queue' is set, in which case the child is already dead and
           we have to fetch everything it sent during its live time. */

        if ( ! empty_queue )
        {
            struct timeval time_struct;
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
        redraw_canvas_1d( &G_1d.canvas );
        if ( Scale_1d_changed[ X ] )
            redraw_canvas_1d( &G_1d.x_axis );
        if ( Scale_1d_changed[ Y ] )
            redraw_canvas_1d( &G_1d.y_axis );
    }

    if ( dim & 2 )
    {
        if ( Need_2d_redraw )
            redraw_canvas_2d( &G_2d.canvas );
        if ( Scale_2d_changed[ X ] )
            redraw_canvas_2d( &G_2d.x_axis );
        if ( Scale_2d_changed[ Y ] )
            redraw_canvas_2d( &G_2d.y_axis );
        if ( Scale_2d_changed[ Z ] )
            redraw_canvas_2d( &G_2d.z_axis );
        if ( Need_cut_redraw )
            redraw_all_cut_canvases( );
    }
}


/*----------------------------------------------------------------*
 * Function examines the new data by looking at the first item
 * and calls the appropriate functions for dealing with the data.
 *----------------------------------------------------------------*/

static void
unpack_and_accept( int          dim,
                   const char * ptr )
{
    /* The first item of a new data package indicates the amount of data
       sets that needs handling. When the first item is a negative number
       it isn't data to be displayed but special commands for clearing
       curves, scale changes etc. */

    int nsets;
    memcpy( &nsets, ptr, sizeof nsets );
    ptr += sizeof nsets;

    if ( nsets < 0 )                /* special for clear curve commands etc. */
    {
        other_data_request( dim, nsets, ptr );
        return;
    }

    /* Accept the new data for the different types of data sets */

    for ( int i = 0; i < nsets; i++ )
    {
        long x_index;
        memcpy( &x_index, ptr, sizeof x_index );
        ptr += sizeof x_index;

        long y_index = 0;
        if ( dim == DATA_2D )
        {
            memcpy( &y_index, ptr, sizeof y_index );
            ptr += sizeof y_index;
        }

        long curve;
        memcpy( &curve, ptr, sizeof curve );
        ptr += sizeof curve;

        Var_Type_T type;
        memcpy( &type, ptr, sizeof type );
        ptr += sizeof type;

        const char * ptr_next = NULL;
        long len;

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

        default :
            fsc2_impossible( );       /* This can't happen... */
        }

        if ( dim == DATA_1D )
            accept_1d_data( x_index, curve, type, ptr );
        else
            accept_2d_data( x_index, y_index, curve, type, ptr );

        ptr = ptr_next;
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_clear_curve( int          dim,
                    const char * ptr )
{
    long count;
    memcpy( &count, ptr, sizeof count ); /* length of list of curves */
    ptr += sizeof count;

    for ( long i = 0; i < count; i++ )
    {
        long ca;
        memcpy( &ca, ptr, sizeof ca );    /* current curve number */
        ptr += sizeof ca;

        if ( dim == DATA_1D )
            clear_curve_1d( ca );
        else
        {
            clear_curve_2d( ca );
            Need_2d_redraw = true;
            cut_clear_curve( ca );
            Need_cut_redraw = true;
        }
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_change_scale( int          dim,
                     const char * ptr )
{
    int is_set;
    memcpy( &is_set, ptr, sizeof is_set );  /* flags */
    ptr += sizeof is_set;

    if ( dim == DATA_1D )
    {
        change_scale_1d( is_set, ( void * ) ptr );
        Scale_1d_changed[ X ] = Scale_1d_changed[ Y ] = true;
    }
    else
    {
        change_scale_2d( is_set, ( void * ) ptr );
        Scale_2d_changed[ X ] =
            Scale_2d_changed[ Y ] =
            Scale_2d_changed[ Z ] = true;
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_vert_rescale( int dim )
{
    if ( dim == DATA_1D )
        fs_vert_rescale_1d( );
    else
        fs_vert_rescale_2d( );
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_change_label( int          dim,
                     const char * ptr )
{
    char *label[ 3 ];

    for ( long i = X; i <= ( dim == DATA_1D ? Y : Z ); i++ )
    {
        long length;
        memcpy( &length, ptr, sizeof length );
        ptr += sizeof length;

        label[ i ] = ( char * ) ptr;
        ptr += length;
    }

    if ( dim == DATA_1D )
    {
        change_label_1d( label );
        Scale_1d_changed[ X ] = Scale_1d_changed[ Y ] = true;
    }
    else
    {
        change_label_2d( label );
        Scale_2d_changed[ X ] =
            Scale_2d_changed[ Y ] =
                Scale_2d_changed[ Z ] = true;
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_change_points( int          dim,
                      const char * ptr)
{
    if ( dim == DATA_1D )
    {
        long resc_arg;

        memcpy( &resc_arg, ptr, sizeof resc_arg );
        rescale_1d( resc_arg );
    }
    else
    {
        long resc_args[ 2 ];

        memcpy( resc_args, ptr, sizeof resc_args );
        rescale_2d( resc_args );
        Need_2d_redraw = Scale_2d_changed[ X ] =
            Scale_2d_changed[ Y ] = true;
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_set_marker( int          dim,
                   const char * ptr )
{
    if ( dim == DATA_1D )
    {
        long position;
        memcpy( &position, ptr, sizeof position );
        ptr += sizeof position;

        long color;
        memcpy( &color, ptr, sizeof color );

        if ( position >= G_1d.nx && G.mode == NORMAL_DISPLAY )
            rescale_1d( position );
        set_marker_1d( position, color );
    }
    else
    {
        long x_pos;
        memcpy( &x_pos, ptr, sizeof x_pos );
        ptr += sizeof x_pos;

        long y_pos;
        memcpy( &y_pos, ptr, sizeof y_pos );
        ptr += sizeof y_pos;

        long color;
        memcpy( &color, ptr, sizeof color );
        ptr += sizeof color;

        long count;
        memcpy( &count, ptr, sizeof count );

        set_marker_2d( x_pos, y_pos, color, count );
        Need_2d_redraw = true;
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_clear_marker( int          dim,
                     const char * ptr )
{
    if ( dim == DATA_1D )
        remove_markers_1d( );
    else
    {
        long curves[ MAX_CURVES ];
        memcpy( curves, ptr, sizeof curves );

        remove_markers_2d( curves );
        Need_2d_redraw = true;
    }
}


/*---------------------------------------------------------------------*
 *---------------------------------------------------------------------*/

static
void
handle_change_mode( int          dim,
                    const char * ptr )
{
    fsc2_assert( dim == DATA_1D );

    long mode;
    memcpy( &mode, ptr, sizeof mode );
    ptr +=sizeof mode;

    long width;
    memcpy( &width, ptr, sizeof width );

    change_mode( mode, width );
}


/*---------------------------------------------------------------------*
 * Function for handling special commands that come disguised as data
 * (commands for clearing curves, changing scales etc). The type of
 * the command is determined and the appropriate functions are called.
 *---------------------------------------------------------------------*/

static
void
other_data_request( int          dim,
                    int          type,
                    const char * ptr )
{
    switch( type )
    {
        case D_CLEAR_CURVE :                     /* clear curve command */
            handle_clear_curve( dim, ptr );
            break;

        case D_CHANGE_SCALE :                       /* change scale command */
            handle_change_scale( dim, ptr );
            break;

        case D_VERT_RESCALE :                       /* change vertical scale */
            handle_vert_rescale( dim );
            break;

        case D_CHANGE_LABEL :                 /* change label command */
            handle_change_label( dim, ptr );
            break;

        case D_CHANGE_POINTS :                /* rescale command */
            handle_change_points( dim, ptr );
            break;

        case D_SET_MARKER :
            handle_set_marker( dim, ptr );
            break;

        case D_CLEAR_MARKERS :
            handle_clear_marker( dim, ptr );
            break;

        case D_CHANGE_MODE :
            handle_change_mode( dim, ptr );
            break;

        default :
            fsc2_impossible( );        /* This can't happen... */
    }
}


/*--------------------------------------------------------*
 * Function for storing and displaying of new data points
 * for 1D graphics (when normal display mode is used).
 *--------------------------------------------------------*/

static void
accept_1d_data( long         x_index,
                long         curve,
                Var_Type_T   type,
                const char * ptr )
{
#ifndef NDEBUG
    /* Test if the curve number is OK */

    if ( curve >= G_1d.nc )
    {
        eprint( FATAL, false, "Internal error detected at %s:%d, there is no "
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

    long len = get_number_of_new_points( &ptr, type );
    long end_index = x_index + len;

    /* If the number of points exceeds the size of the arrays for the curves
       we have to increase the sizes for all curves. But take care: While
       x_index + len may be greater than G_1d.nx, x_index can still be smaller
       than G_1d.nx ! */

    if ( end_index > G_1d.nx )
    {
        Curve_1d_T * cv = G_1d.curve[ 0 ];
        for ( long i = 0 ; i < G_1d.nc; cv = G_1d.curve[ ++i ] )
        {
            cv->points = T_realloc( cv->points,
                                    end_index * sizeof *cv->points );
            cv->xpoints = T_realloc( cv->xpoints,
                                     end_index * sizeof *cv->xpoints );

            Scaled_Point_T * sp = cv->points + G_1d.nx;
            for ( long j = G_1d.nx; j < end_index; sp++, j++ )
                sp->exist = false;

            if ( G_1d.is_fs )
                cv->s2d[ X ] = ( double ) ( G_1d.canvas.w - 1 ) /
                               ( double ) ( x_index + len - 1 );
        }

        Scale_1d_changed[ X ] |= G_1d.is_fs;
    }

    /* Find maximum and minimum of old and new data and, if the minimum or
       maximum has changed, rescale all old scaled data*/

    double old_rw_min = G_1d.rw_min;

    if ( get_new_extrema( &G_1d.rw_max, &G_1d.rw_min, ptr, len, type ) )
    {
        double new_rwc_delta_y = G_1d.rw_max - G_1d.rw_min;

        if ( G_1d.is_scale_set )
        {
            double factor = G_1d.rwc_delta[ Y ] / new_rwc_delta_y;
            double offset = ( old_rw_min - G_1d.rw_min ) / new_rwc_delta_y;

            Curve_1d_T * cv = G_1d.curve[ 0 ];
            for ( long i = 0; i < G_1d.nc; cv = G_1d.curve[ ++i ] )
            {
                Scaled_Point_T * sp = cv->points;
                for ( long count = cv->count; count > 0; sp++ )
                    if ( sp->exist )
                    {
                        sp->v = factor * sp->v + offset;
                        count--;
                    }

                if ( G_1d.is_fs )
                    continue;

                cv->s2d[ Y ]  /= factor;
                cv->shift[ Y ] = factor * cv->shift[ Y ] - offset;
            }
        }

        /* If the data had not been scaled to [0,1] yet and the maximum value
           isn't identical to the minimum anymore do the scaling now */

        if ( ! G_1d.is_scale_set && G_1d.rw_max != G_1d.rw_min )
        {
            double factor = 1.0 / new_rwc_delta_y;
            double offset = - G_1d.rw_min / new_rwc_delta_y;

            Curve_1d_T * cv = G_1d.curve[ 0 ];
            for ( long i = 0; i < G_1d.nc; cv = G_1d.curve[ ++i ] )
            {
                Scaled_Point_T * sp = cv->points;
                for ( long count = cv->count; count > 0; sp++ )
                    if ( sp->exist )
                    {
                          sp->v = factor * sp->v + offset;
                          count--;
                    }
            }

            G_1d.is_scale_set = true;
            Scale_1d_changed[ X ] = true;
        }

        Scale_1d_changed[ Y ] = true;
        G_1d.rwc_delta[ Y ] = new_rwc_delta_y;
    }

    /* Now we're finished with rescaling and can set the new number of points
       if necessary */

    if ( end_index > G_1d.nx )
        G_1d.nx = end_index;

    /* Include the new data into the scaled data */

    const char * cur_ptr = ptr;
    Scaled_Point_T * sp = G_1d.curve[ curve ]->points + x_index;
    for ( long i = x_index; i < end_index; sp++, i++ )
    {
        double data;

        if ( type & ( INT_VAR | INT_ARR ) )
        {
            long ldata;
            memcpy( &ldata, cur_ptr, sizeof ldata );
            cur_ptr += sizeof ldata;
            data = ldata;
        }
        else
        {
            memcpy( &data, cur_ptr, sizeof data );
            cur_ptr += sizeof data;
        }

        if ( G_1d.is_scale_set )
            sp->v = ( data - G_1d.rw_min ) / G_1d.rwc_delta[ Y ];
        else
            sp->v = data;

        /* Increase the point count if the point is new and mark it as set */

        if ( ! sp->exist )
        {
            G_1d.curve[ curve ]->count++;
            sp->exist = true;
        }
    }

    /* Calculate new points for display (unless no scale is set yet) */

    if ( G_1d.is_scale_set )
    {
        G_1d.rwc_start[ Y ] = G_1d.rw_min;

        /* If the scale did not change redraw only the current curve,
           otherwise all curves */

        if ( ! ( Scale_1d_changed[ X ] || Scale_1d_changed[ Y ] ) )
            recalc_XPoints_of_curve_1d( G_1d.curve[ curve ] );
        else
            for ( long i = 0; i < G_1d.nc; i++ )
                recalc_XPoints_of_curve_1d( G_1d.curve[ i ] );
    }
}


/*--------------------------------------------------------*
 * Function for handling new data for the 1D display when
 * "sliding window" mode is used.
 *--------------------------------------------------------*/

static void
accept_1d_data_sliding( long         curve,
                        Var_Type_T   type,
                        const char * ptr )
{
    /* Get the amount of new data and a pointer to the start of the data */

    long len = get_number_of_new_points( &ptr, type );

    /* Find maximum and minimum of old and new data and, if the minimum or
       maximum has changed, rescale all old scaled data*/

    double old_rw_min = G_1d.rw_min;

    if ( get_new_extrema( &G_1d.rw_max, &G_1d.rw_min, ptr, len, type ) )
    {
        double new_rwc_delta_y = G_1d.rw_max - G_1d.rw_min;

        if ( G_1d.is_scale_set )
        {
            double factor = G_1d.rwc_delta[ Y ] / new_rwc_delta_y;
            double offset = ( old_rw_min - G_1d.rw_min ) / new_rwc_delta_y;

            Curve_1d_T * cv = G_1d.curve[ 0 ];
            for ( long i = 0; i < G_1d.nc; cv = G_1d.curve[ ++i ] )
            {
                Scaled_Point_T * sp = cv->points;
                for ( long count = cv->count; count > 0; sp++ )
                {
                    sp->v = factor * sp->v + offset;
                    count--;
                }

                if ( G_1d.is_fs )
                    continue;

                cv->s2d[ Y ]  /= factor;
                cv->shift[ Y ] = factor * cv->shift[ Y ] - offset;
            }
        }

        /* If the data had not been scaled to [0,1] yet and the maximum value
           isn't identical to the minimum anymore do the scaling now */

        if ( ! G_1d.is_scale_set && G_1d.rw_max != G_1d.rw_min )
        {
            double factor = 1.0 / new_rwc_delta_y;
            double offset = - G_1d.rw_min / new_rwc_delta_y;

            Curve_1d_T * cv = G_1d.curve[ 0 ];
            for ( long i = 0; i < G_1d.nc; cv = G_1d.curve[ ++i ] )
            {
                Scaled_Point_T * sp = cv->points;
                for ( long count = cv->count; count > 0; sp++, count-- )
                    sp->v = factor * sp->v + offset;
            }

            Scale_1d_changed[ X ] = true;
            G_1d.is_scale_set = true;
        }

        Scale_1d_changed[ Y ] = true;
        G_1d.rwc_delta[ Y ] = new_rwc_delta_y;
    }

    /* Now we're finished with rescaling we can deal with the new data. First
       we must drop new points that wouldn't fit into the window (in case that
       we get too many new points). */

    if ( len > G_1d.nx )
    {
        fsc2_assert( type ==  INT_ARR || type == FLOAT_ARR );
        ptr +=   ( len - G_1d.nx )
               * ( type == INT_ARR ? sizeof( long ) : sizeof( double ) );
        len = G_1d.nx;
    }

    /* Remove old points of all curves that wouldn't fit into the window
       anymore */

    if ( G_1d.curve[ curve ]->count + len > G_1d.nx )
    {
        long shift = len + G_1d.curve[ curve ]->count - G_1d.nx;

        for ( long i = 0; i < G_1d.nc; i++ )
        {
            Curve_1d_T * cv = G_1d.curve[ i ];
            if ( shift >= cv->count )
            {
                Scaled_Point_T * sp = cv->points;
                for ( long count = cv->count; count > 0; sp++, count-- )
                    sp->exist = false;
                cv->count = 0;
            }
            else
            {
                Scaled_Point_T * sp1 = cv->points,
                               * sp2 = sp1 + shift;
                long count;
                for ( count = shift; count < cv->count; sp1++, sp2++, count++ )
                    sp1->v = sp2->v;

                for ( count -= shift, sp2 -= shift; count < cv->count;
                      sp2++, count++ )
                    sp2->exist = 0;

                cv->count -= shift;
            }
        }

        Marker_1d_T * m = G_1d.marker_1d,
                    *mn,
                    *mp = NULL;
        while ( m )
        {
            m->x_pos -= shift;
            if ( m->x_pos < 0 )
            {
                XFreeGC( G.d, m->gc );
                mn = m->next;
                if ( mp != NULL )
                    mp->next = mn;
                if ( m == G_1d.marker_1d )
                    G_1d.marker_1d = mn;
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

    Curve_1d_T * cv = G_1d.curve[ curve ];
    const char * cur_ptr = ptr;
    Scaled_Point_T * sp = cv->points + cv->count;

    for ( long i = 0; i < len; sp++, i++ )
    {
        double data;

        if ( type & ( INT_VAR | INT_ARR ) )
        {
            long ldata;
            memcpy( &ldata, cur_ptr, sizeof ldata );
            cur_ptr += sizeof ldata;
            data = ldata;
        }
        else
        {
            memcpy( &data, cur_ptr, sizeof data );
            cur_ptr += sizeof data;
        }

        if ( G_1d.is_scale_set )
            sp->v = ( data - G_1d.rw_min ) / G_1d.rwc_delta[ Y ];
        else
            sp->v = data;

        sp->exist = true;
    }

    cv->count += len;

    /* Calculate new points for display (unless no scale is set yet) */

    if ( G_1d.is_scale_set )
    {
        G_1d.rwc_start[ Y ] = G_1d.rw_min;

        /* If the scale did not change recalculate the points of the current
           curve only, otherwise the points of all curves */

        if ( ! ( Scale_1d_changed[ X ] || Scale_1d_changed[ Y ] ) )
            recalc_XPoints_of_curve_1d( G_1d.curve[ curve ] );
        else
            for ( long i = 0; i < G_1d.nc; i++ )
                recalc_XPoints_of_curve_1d( G_1d.curve[ i ] );
    }
}



/*----------------------------------------------------------------*
 * Function for storing and displaying of new data points for the
 * 2D display (sorry if it's a bit hard to understand, but quite
 * a bit of work has gone into getting it as fast as possible...)
 *----------------------------------------------------------------*/

static void accept_2d_data( long         x_index,
                            long         y_index,
                            long         curve,
                            Var_Type_T   type,
                            const char * ptr )
{
#ifndef NDEBUG
    /* Test if the curve number is OK */

    if ( curve >= G_2d.nc )
    {
        eprint( FATAL, false, "Internal error detected at %s:%d, there is no "
                "curve %ld.\n", __FILE__, __LINE__, curve + 1 );
        THROW( EXCEPTION );
    }
#endif

    bool size_changed = false;

    /* Get number of new data points and a pointer to the start of the data */

    long x_len = get_number_of_new_points( &ptr, type );
    long y_len = 0;

    if ( type & ( INT_REF | FLOAT_REF ) )
    {
        memcpy( &y_len, ptr, sizeof y_len );
        y_len--;
    }

    Curve_2d_T * cv = G_2d.curve_2d[ curve ];

    /* Check if the new data fit into the already allocated memory. If not
       extend the memory area and figure out if this requires redrawing of
       the areas of the axes and the cut window. */

    if ( x_index + x_len > G_2d.nx )
    {
        if ( y_index + y_len >= G_2d.ny )
        {
            Need_cut_redraw |= incr_x_and_y( x_index, x_len, y_index + y_len);
            if (    ( cv->active && cv->is_fs )
                 || (    G_2d.active_curve != -1
                      && G_2d.curve_2d[ G_2d.active_curve ]->is_fs ) )
                Scale_2d_changed[ X ] = Scale_2d_changed[ Y ] = true;
        }
        else
        {
            Need_cut_redraw |= incr_x( x_index, x_len );
            Scale_2d_changed[ X ] |=
                              ( cv->active && cv->is_fs )
                           || (    G_2d.active_curve != -1
                                && G_2d.curve_2d[ G_2d.active_curve ]->is_fs );
        }

        size_changed = true;
    }
    else if ( y_index + y_len >= G_2d.ny )
    {
        Need_cut_redraw |= incr_y( y_index + y_len );
        Scale_2d_changed[ Y ] |=
                              ( cv->active && cv->is_fs )
                           || (    G_2d.active_curve != -1
                                && G_2d.curve_2d[ G_2d.active_curve ]->is_fs );
        size_changed = true;
    }

    /* Find maximum and minimum of old and new data. If the minimum or
       maximum changed, (re)scale all old data */

    double old_rw_min = cv->rw_min;

    if ( get_new_extrema( &cv->rw_max, &cv->rw_min, ptr, x_len, type ) )
    {
        double new_rwc_delta_z = cv->rw_max - cv->rw_min;

        if ( cv->is_scale_set )
        {
            double factor = cv->rwc_delta[ Z ] / new_rwc_delta_z;
            double offset = ( old_rw_min - cv->rw_min ) / new_rwc_delta_z;

            Scaled_Point_T * sp = cv->points;
            for ( long count = cv->count; count > 0; sp++ )
                if ( sp->exist )
                {
                    sp->v = factor * sp->v + offset;
                    count--;
                }

            if ( ! cv->is_fs )
            {
                cv->s2d[ Z ] /= factor;
                cv->shift[ Z ] = factor * cv->shift[ Z ] - offset;
                cv->z_factor /= factor;
            }
        }

        /* If data have not been scaled yet to the interval [0,1] and the
           maximum value isn't identical to the minimum value anymore
           calculate the scaling */

        if ( ! cv->is_scale_set && cv->rw_max != cv->rw_min )
        {
            double factor = 1.0 / new_rwc_delta_z;
            double offset = - cv->rw_min / new_rwc_delta_z;

            Scaled_Point_T *sp = cv->points;
            for ( long count = cv->count; count > 0; sp++ )
                if ( sp->exist )
                {
                    sp->v = factor * sp->v + offset;
                    count--;
                }

            cv->is_scale_set = true;

            if ( cv->active )
                Scale_2d_changed[ X ] = Scale_2d_changed[ Y ] = true;
        }

        Scale_2d_changed[ Z ] |= cv->active && cv->is_fs;
        Need_cut_redraw |= cut_data_rescaled( curve, cv->rw_min, cv->rw_max );
        cv->rwc_delta[ Z ] = new_rwc_delta_z;
    }

    /* Now we're finished with rescaling and can set the new number of points
       if necessary */

    if ( x_index + x_len > G_2d.nx )
        G_2d.nx = x_index + x_len;
    if ( y_index + y_len >= G_2d.ny )
        G_2d.ny = y_index + y_len + 1;

    /* Include the new data into the scaled data */

    if ( type & ( INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR ) )
    {
        const char * cur_ptr = ptr;
        long end_index = x_index + x_len;
        Scaled_Point_T * sp = cv->points + y_index * G_2d.nx + x_index;

        for ( long i = x_index; i < end_index; sp++, i++ )
        {
            double data;

            if ( type & ( INT_VAR | INT_ARR ) )
            {
                long ldata;
                memcpy( &ldata, cur_ptr, sizeof ldata );
                cur_ptr += sizeof ldata;
                data = ldata;
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
                sp->exist = true;
            }
        }

        /* Tell the cross section handler about the new data, its return value
           indicates if the cut graphics needs to be redrawn */

        Need_cut_redraw |= cut_new_points( curve, x_index, y_index, x_len );
    }
    else        /* 2-dimensional data field is to be included */
    {
        const char * cur_ptr = ptr += sizeof y_len;
        long end_index = y_index + y_len;

        for ( long i = y_index; i <= end_index; i++ )
        {
            Scaled_Point_T * sp = cv->points + i * G_2d.nx + x_index;
            memcpy( &x_len, cur_ptr, sizeof x_len );
            cur_ptr += sizeof x_len;

            for ( long j = 0; j < x_len; sp++, j++ )
            {
                double data;

                if ( type == INT_REF )
                {
                    long ldata;
                    memcpy( &ldata, cur_ptr, sizeof ldata );
                    cur_ptr += sizeof ldata;
                    data = ldata;
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
                    sp->exist = true;
                }
            }

            /* Tell the cross section handler about the new data, its return
               value indicates if the cut graphics needs to be redrawn */

            Need_cut_redraw |= cut_new_points( curve, x_index, i, x_len );
        }
    }

    /* We're done when nothing is to be drawn because no scaling for the curve
       is set yet */

    if ( ! cv->is_scale_set )
        return;

    /* Set flags that indicate what needs to be recalculated and what needs
       to be redrawn */

    cv->rwc_start[ Z ] = cv->rw_min;

    /* Since new points were included a recalculation of the current curve
       is required */

    cv->needs_recalc = true;

    /* All other curves only need to be recalculated if the x- or y-size
       has changed (and the curve is scaled at all) */

    for ( long i = 0; i < G_2d.nc; i++ )
        if ( G_2d.curve_2d[ i ]->is_scale_set && i != curve )
            G_2d.curve_2d[ i ]->needs_recalc |= size_changed;

    /* We need a redraw of the canvas if the currently displayed curve
       needs a recalculation because either the size(s) changed and ist's
       in full scale mode or because new data points got added to the curve. */

    if ( G_2d.active_curve != -1 )
        Need_2d_redraw |=    G_2d.curve_2d[ G_2d.active_curve ]->needs_recalc
                          && (    G_2d.curve_2d[ G_2d.active_curve ]->is_fs
                               || G_2d.curve_2d[ G_2d.active_curve ] == cv );
}


/*---------------------------------------------------------------------*
 * Function used by all functions that unpack new data to be displayed
 * to figure out how many new data there really are. When invoked *ptr
 * points to the start of the new data set, and 'type' indicates the
 * type of the new data (integer or floating point numbers, 1D or
 * multi-dimensional array). It returns the number of of new data
 * points and '*ptr' is pointing to the start of the data within the
 * set (i.e. after the header) when the function returns.
 *---------------------------------------------------------------------*/

static
long
get_number_of_new_points( const char ** ptr,
                          Var_Type_T    type )
{
    long len = 0;
    const char *ptr_2d = *ptr;
    long y_len;

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
            ptr_2d += sizeof y_len;

            for ( long i = 0; i < y_len; i++ )
            {
                long x_len;
                memcpy( &x_len, ptr_2d, sizeof x_len );
                ptr_2d += sizeof x_len + x_len * sizeof( long );

                len = l_max( len, x_len );
            }
            break;

        case FLOAT_REF :
            memcpy( &y_len, *ptr, sizeof len );
            ptr_2d += sizeof y_len;

            for ( long i = 0; i < y_len; i++ )
            {
                long x_len;
                memcpy( &x_len, ptr_2d, sizeof x_len );
                ptr_2d += sizeof x_len + x_len * sizeof( double );

                len = l_max( len, x_len );
            }
            break;

        default :
            fsc2_impossible( );        /* This can't happen... */
    }

    if ( len <= 0 )
        fsc2_impossible( );

    return len;
}


/*----------------------------------------------------------*
 * Determines the new maximum and minimum value of the data
 * set and returns if the maximum or minimum changed.
 *----------------------------------------------------------*/

static bool
get_new_extrema( double     * max,
                 double     * min,
                 const char * ptr,
                 long         len,
                 Var_Type_T   type )
{
    double old_max = *max;
    double old_min = *min;

    if ( type & ( INT_VAR | FLOAT_VAR | INT_ARR | FLOAT_ARR ) )
    {
        for ( long i = 0; i < len; i++ )
        {
            double data;

            if ( type & ( INT_VAR | INT_ARR ) )
            {
                long ldata;
                memcpy( &ldata, ptr, sizeof ldata );
                ptr += sizeof ldata;
                data = ldata;
            }
            else
            {
                memcpy( &data, ptr, sizeof data );
                ptr += sizeof data;
            }

            *max = d_max( data, *max );
            *min = d_min( data, *min );
        }
    }
    else if ( type == INT_REF )
    {
        long y_len;
        memcpy( &y_len, ptr, sizeof y_len );
        ptr += sizeof y_len;

        for ( long i = 0; i < y_len; i++ )
        {
            double data;

            long x_len;
            memcpy( &x_len, ptr, sizeof x_len );
            ptr += sizeof x_len;

            for ( long j = 0; j < x_len; j++ )
            {
                long ldata;
                memcpy( &ldata, ptr, sizeof ldata );
                ptr += sizeof ldata;

                data = ldata;
                *max = d_max( data, *max );
                *min = d_min( data, *min );
            }
        }
    }
    else if ( type == FLOAT_REF )
    {
        long y_len;
        memcpy( &y_len, ptr, sizeof y_len );
        ptr += sizeof y_len;

        for ( long i = 0; i < y_len; i++ )
        {
            long x_len;
            memcpy( &x_len, ptr, sizeof x_len );
            ptr += sizeof x_len;

            for ( long j = 0; j < x_len; j++ )
            {
                double data;
                memcpy( &data, ptr, sizeof data );
                ptr += sizeof data;

                *max = d_max( data, *max );
                *min = d_min( data, *min );
            }
        }
    }

    return *max > old_max || *min < old_min;
}


/*-------------------------------------------------*
 * Increments the number of 2D data in x-direction
 *-------------------------------------------------*/

static bool
incr_x( long x_index,
        long len )
{
    long new_Gnx = x_index + len;
    long new_num = new_Gnx * G_2d.ny;

    for ( long i = 0; i < G_2d.nc; i++ )
    {
        Curve_2d_T * cv = G_2d.curve_2d[ i ];

        Scaled_Point_T * old_points = cv->points;
        Scaled_Point_T * sp = cv->points = T_malloc( new_num * sizeof *sp );

        for ( long j = 0; j < G_2d.ny; j++ )
        {
            memcpy( sp, old_points + j * G_2d.nx, G_2d.nx * sizeof *sp );
            sp += G_2d.nx;
            for ( long k = G_2d.nx; k < new_Gnx; sp++, k++ )
                sp->exist = false;
        }

        T_free( old_points );

        cv->xpoints = T_realloc( cv->xpoints, new_num * sizeof *cv->xpoints );

        if ( cv->is_fs )
            cv->s2d[ X ] = ( double ) ( G_2d.canvas.w - 1 ) /
                           ( double ) ( new_Gnx - 1 );
    }

    /* Also tell the routines for drawing cuts about the changes and return
       the return value of this function (which indicates if a redraw of the
       cut is needed) */

    return cut_num_points_changed( X, new_Gnx );
}


/*-------------------------------------------------*
 * Increments the number of 2D data in y-direction
 *-------------------------------------------------*/

static bool
incr_y( long y_index )
{
    long new_Gny = y_index + 1;
    long new_num = G_2d.nx * new_Gny;

    for ( long i = 0; i < G_2d.nc; i++ )
    {
        Curve_2d_T * cv = G_2d.curve_2d[ i ];

        cv->points = T_realloc( cv->points, new_num * sizeof *cv->points );
        cv->xpoints = T_realloc( cv->xpoints, new_num * sizeof *cv->xpoints );

        Scaled_Point_T *sp = cv->points + G_2d.ny * G_2d.nx;
        for ( long j = G_2d.ny * G_2d.nx; j < new_num; sp++, j++ )
            sp->exist = false;

        if ( cv->is_fs )
            cv->s2d[ Y ] = ( double ) ( G_2d.canvas.h - 1 ) /
                           ( double ) ( new_Gny - 1 );
    }

    /* Also tell the routines for drawing cuts about the changes and return
       the return value of this function (which indicates if a redraw of the
       cut is needed) */

    return cut_num_points_changed( Y, new_Gny );
}


/*-------------------------------------------------------------*
 * Increments the number of 2D data in both x- and y-direction
 *-------------------------------------------------------------*/

static bool
incr_x_and_y( long x_index,
              long len,
              long y_index )
{
    long new_Gnx = x_index + len;
    long new_Gny = y_index + 1;
    long new_num = new_Gnx * new_Gny;

    for ( long i = 0; i < G_2d.nc; i++ )
    {
        Curve_2d_T * cv = G_2d.curve_2d[ i ];
        Scaled_Point_T * old_points = cv->points;
        Scaled_Point_T * sp = cv->points = T_malloc( new_num * sizeof *sp );

        /* Reorganise the old elements to fit into the new array and clear
           the new elements in the already existing rows */

        for ( long j = 0; j < G_2d.ny; j++ )
        {
            memcpy( sp, old_points + j * G_2d.nx, G_2d.nx * sizeof *sp );
            sp += G_2d.nx;
            for ( long k = G_2d.nx; k < new_Gnx; sp++, k++ )
                sp->exist = false;
        }

        /* Now also set the elements in the comletely new rows to unused */

        for ( long j = new_Gnx * G_2d.ny; j < new_num; sp++, j++ )
            sp->exist = false;

        T_free( old_points );

        cv->xpoints = T_realloc( cv->xpoints, new_num * sizeof *cv->xpoints );

        if ( cv->is_fs )
        {
            cv->s2d[ X ] = ( double ) ( G_2d.canvas.w - 1 ) /
                           ( double ) ( new_Gnx - 1 );
            cv->s2d[ Y ] = ( double ) ( G_2d.canvas.h - 1 ) /
                           ( double ) ( new_Gny - 1 );
        }
    }

    /* Also tell the routines for drawing cuts about the changes and return
       the return value of this function (which indicates if a redraw of the
       cut is needed) */

    bool ret = cut_num_points_changed( X, new_Gnx );
    return ret || cut_num_points_changed( Y, new_Gny );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
