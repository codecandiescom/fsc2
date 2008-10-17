/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2.h"


/*--------------------------------------------------------------*
 * Function does the work for the clear_curve_1d() EDL function
 *--------------------------------------------------------------*/

void
clear_curve_1d( long curve )
{
    long i;
    Scaled_Point_T *sp;


    for ( sp = G_1d.curve[ curve ]->points, i = 0; i < G_1d.nx; sp++, i++ )
        sp->exist = UNSET;
    G_1d.curve[ curve ]->count = 0;
}


/*--------------------------------------------------------------*
 * Function does the work for the clear_curve_2d() EDL function
 *--------------------------------------------------------------*/

void
clear_curve_2d( long curve )
{
    long i;
    Scaled_Point_T *sp;


    for ( sp = G_2d.curve_2d[ curve ]->points, i = 0; i < G_2d.nx * G_2d.ny;
          sp++, i++ )
        sp->exist = UNSET;
    G_2d.curve_2d[ curve ]->count = 0;
}


/*---------------------------------------------------------------*
 * Function does the work for the change_scale_1d() EDL function
 *---------------------------------------------------------------*/

void
change_scale_1d( int    is_set,
                 void * ptr )
{
    double vals[ 4 ];


    memcpy( vals, ptr, sizeof vals );

    if ( is_set & 1 )
        G_1d.rwc_start[ X ] = vals[ 0 ];
    if ( is_set & 2 )
        G_1d.rwc_delta[ X ] = vals[ 1 ];
}


/*---------------------------------------------------------------*
 * Function does the work for the change_scale_2d() EDL function
 *---------------------------------------------------------------*/

void
change_scale_2d( int    is_set,
                 void * ptr )
{
    long i;
    double vals[ 4 ];


    memcpy( vals, ptr, sizeof vals );

    if ( is_set & 1 )
    {
        G_2d.rwc_start[ X ] = vals[ 0 ];
        for ( i = 0; i < G_2d.nc; i++ )
            G_2d.curve_2d[ i ]->rwc_start[ X ] = vals[ 0 ];
    }

    if ( is_set & 2 )
    {
        G_2d.rwc_delta[ X ] = vals[ 1 ];
        for ( i = 0; i < G_2d.nc; i++ )
            G_2d.curve_2d[ i ]->rwc_delta[ X ] = vals[ 1 ];
    }

    if ( is_set & 4 )
    {
        G_2d.rwc_start[ Y ] = vals[ 2 ];
        for ( i = 0; i < G_2d.nc; i++ )
            G_2d.curve_2d[ i ]->rwc_start[ Y ] = vals[ 2 ];
    }

    if ( is_set & 8 )
    {
        G_2d.rwc_delta[ Y ] = vals[ 3 ];
        for ( i = 0; i < G_2d.nc; i++ )
            G_2d.curve_2d[ i ]->rwc_delta[ Y ] = vals[ 3 ];
    }
}


/*---------------------------------------------------------------*
 * Function does the work for the change_label_1d() EDL function
 *---------------------------------------------------------------*/

void
change_label_1d( char ** label )
{
    if ( *label[ X ] != '\0' )
    {
        T_free( G_1d.label[ X ] );
        G_1d.label[ X ] = T_strdup( label[ X ] );
        redraw_canvas_1d( &G_1d.x_axis );
    }

    if ( *label[ Y ] != '\0' )
    {
        if ( G_1d.label[ Y ] != NULL )
        {
            G_1d.label[ Y ] = T_free( G_1d.label[ Y ] );
            if ( G.font != NULL )
                XFreePixmap( G.d, G_1d.label_pm );
        }

        G_1d.label[ Y ] = T_strdup( label[ Y ] );
        if ( G.font != NULL )
        {
            create_label_pixmap( &G_1d.y_axis, Y, G_1d.label[ Y ] );
            redraw_canvas_1d( &G_1d.y_axis );
        }
    }
}


/*---------------------------------------------------------------*
 * Function does the work for the change_label_2d() EDL function
 *---------------------------------------------------------------*/

void
change_label_2d( char ** label )
{
    int coord;


    if ( *label[ X ] != '\0' )
    {
        if ( G_2d.label[ X ] != NULL )
        {
            G_2d.label[ X ] = T_free( G_2d.label[ X ] );
            if ( G_cut.is_shown && G_cut.cut_dir == X )
                XFreePixmap( G.d, G_2d.label_pm[ Z + 3 ] );
        }
        G_2d.label[ X ] = T_strdup( label[ X ] );

        redraw_canvas_2d( &G_2d.x_axis );

        if ( G_cut.is_shown )
        {
            if ( G_cut.cut_dir == X )
            {
                if ( G_2d.label[ X ] != NULL && G.font != NULL )
                    create_label_pixmap( &G_2d.cut_z_axis, Z,
                                         G_2d.label[ X ] );
                redraw_cut_axis( Z );
            }
            else
                redraw_cut_axis( X );
        }
    }

    for ( coord = Y; coord <= Z; coord++ )
        if ( *label[ coord ] != '\0' )
        {
            if ( G_2d.label[ coord ] != NULL )
            {
                T_free( G_2d.label[ coord ] );
                if ( G.font != NULL )
                    XFreePixmap( G.d, G_2d.label_pm[ coord ] );
            }
            G_2d.label[ coord ] = T_strdup( label[ coord ] );

            if ( G.font != NULL )
                create_label_pixmap( coord == Y ? &G_2d.y_axis : &G_2d.z_axis,
                                     coord, G_2d.label[ coord ] );
            redraw_canvas_2d( coord == Y ? &G_2d.y_axis : &G_2d.z_axis );

            if ( G_cut.is_shown )
            {
                if ( coord == Y )
                {
                    if ( G_cut.cut_dir == Y )
                    {
                        if ( G_2d.label[ Y ] != NULL && G.font != NULL )
                        {
                            G_2d.label_pm[ Z + 3 ] = G_2d.label_pm[ Y ];
                            G_2d.label_w[ Z + 3 ]  = G_2d.label_w[ Y ];
                            G_2d.label_h[ Z + 3 ]   = G_2d.label_h[ Y ];
                        }
                        redraw_cut_axis( Z );
                    }
                    else
                        redraw_cut_axis( X );
                }

                if ( coord == Z )
                {
                    if ( G_2d.label[ Z ] != NULL && G.font != NULL )
                    {
                        G_2d.label_pm[ Y + 3 ] = G_2d.label_pm[ Z ];
                        G_2d.label_w[ Y + 3 ]  = G_2d.label_w[ Z ];
                        G_2d.label_h[ Y + 3 ]  = G_2d.label_h[ Z ];
                    }
                    redraw_cut_axis( Y );
                }
            }
        }
}


/*----------------------------------------------------------*
 * Function does the work for the rescale_1d() EDL function
 *----------------------------------------------------------*/

void
rescale_1d( long new_nx )
{
    long i, k, count;
    long max_x = 0;
    Scaled_Point_T *sp;
    Marker_1d_T *m;


    /* Return immediately on negative values, they're silently ignored */

    if ( new_nx < 0 )
        return;

    /* Find the maximum x-index currently used by a point or a marker */

    for ( k = 0; k < G_1d.nc; k++ )
        for ( count = G_1d.curve[ k ]->count, sp = G_1d.curve[ k ]->points,
              i = 0; count > 0; sp++, i++ )
            if ( sp->exist )
            {
                if( i > max_x )
                    max_x = i;
                count--;
            }

    for ( m = G_1d.marker_1d; m != NULL; m = m->next )
        if ( m->x_pos > max_x )
            max_x = m->x_pos;

    if ( max_x != 0 )
        max_x++;
    else
        max_x = MIN_1D_X_POINTS;

    /* Make sure we don't rescale to less than the current number of
       points (or the minimum value, if larger) */

    if ( new_nx < MIN_1D_X_POINTS )
    {
        if ( max_x < MIN_1D_X_POINTS )
            max_x = MIN_1D_X_POINTS;
    }
    else if ( new_nx > max_x )
        max_x = new_nx;

    for ( k = 0; k < G_1d.nc; k++ )
    {
        G_1d.curve[ k ]->points = T_realloc( G_1d.curve[ k ]->points,
                                     max_x * sizeof *G_1d.curve[ k ]->points );
        G_1d.curve[ k ]->xpoints = T_realloc( G_1d.curve[ k ]->xpoints,
                                    max_x * sizeof *G_1d.curve[ k ]->xpoints );

        for ( i = G_1d.nx, sp = G_1d.curve[ k ]->points + i; i < max_x;
              sp++, i++ )
            sp->exist = UNSET;
    }

    G_1d.nx = max_x;

    for ( k = 0; k < G_1d.nc; k++ )
    {
        if ( G_1d.is_fs )
        {
            G_1d.curve[ k ]->s2d[ X ] = ( double ) ( G_1d.canvas.w - 1 )
                                        / ( double ) ( G_1d.nx - 1 );
            if ( G_1d.is_scale_set )
                recalc_XPoints_of_curve_1d( G_1d.curve[ k ] );
        }
    }
}


/*----------------------------------------------------------*
 * Function does the work for the rescale_2d() EDL function
 *----------------------------------------------------------*/

void
rescale_2d( long * new_dims )
{
    long i, j, k, l, count;
    long max_x = 0,
         max_y = 0;
    Scaled_Point_T *sp, *old_sp, *osp;
    bool need_cut_redraw = UNSET;
    long new_nx, new_ny;


    new_nx = new_dims[ X ];
    new_ny = new_dims[ Y ];

    if ( new_nx < 0 && new_ny < 0 )
        return;

    /* Find the maximum x and y index used until now */

    for ( k = 0; k < G_2d.nc; k++ )
        for ( j = 0, count = G_2d.curve_2d[ k ]->count,
              sp = G_2d.curve_2d[ k ]->points; count > 0; j++ )
            for ( i = 0; i < G_2d.nx && count > 0; sp++, i++ )
                if ( sp->exist )
                {
                    max_y = j;
                    if ( i > max_x )
                        max_x = i;
                    count--;
                }

    if ( max_x != 0 )
        max_x++;
    else
        max_x = MIN_2D_X_POINTS;

    if ( max_y != 0 )
         max_y++;
    else
        max_y = MIN_2D_Y_POINTS;

    /* Figure out the correct new values - at least the current points must
       still be displayable and at least the minimum sizes must be kept */

    if ( new_nx < 0 )
        new_nx = G_2d.nx;
    if ( new_nx < max_x )
        new_nx = max_x;

    if ( new_ny < 0 )
        new_ny = G_2d.ny;
    if ( new_ny < max_y )
        new_ny = max_y;

    /* Now we're in for the real fun... */

    for ( k = 0; k < G_2d.nc; k++ )
    {
        /* Reorganize the old elements to fit into the new array and clear
           the the new elements in the already existing rows */

        old_sp = osp = G_2d.curve_2d[ k ]->points;
        sp = G_2d.curve_2d[ k ]->points = T_malloc(   new_nx * new_ny
                                                    * sizeof *sp );

        for ( j = 0; j < l_min( G_2d.ny, new_ny ); j++, osp += G_2d.nx )
        {
            memcpy( sp, osp, l_min( G_2d.nx, new_nx ) * sizeof *sp );
            if ( G_2d.nx < new_nx )
                for ( l = G_2d.nx, sp += G_2d.nx; l < new_nx; l++, sp++ )
                    sp->exist = UNSET;
            else
                sp += new_nx;
        }

        for ( ; j < new_ny; j++ )
            for ( l = 0; l < new_nx; l++, sp++ )
                sp->exist = UNSET;

        T_free( old_sp );

        G_2d.curve_2d[ k ]->xpoints = T_realloc( G_2d.curve_2d[ k ]->xpoints,
                                         new_nx * new_ny
                                       * sizeof *G_2d.curve_2d[ k ]->xpoints );

        if ( G_2d.curve_2d[ k ]->is_fs )
        {
            G_2d.curve_2d[ k ]->s2d[ X ] = ( double ) ( G_2d.canvas.w - 1 )
                                           / ( double ) ( new_nx - 1 );
            G_2d.curve_2d[ k ]->s2d[ Y ] = ( double ) ( G_2d.canvas.h - 1 )
                                           / ( double ) ( new_ny - 1 );
        }
    }

    if ( G_2d.nx != new_nx )
         need_cut_redraw = cut_num_points_changed( X, new_nx );
    if ( G_2d.ny != new_ny )
        need_cut_redraw |= cut_num_points_changed( Y, new_ny );

    G_2d.nx = new_nx;
    G_2d.ny = new_ny;

    for ( k = 0; k < G_2d.nc; k++ )
        recalc_XPoints_of_curve_2d( G_2d.curve_2d[ k ] );

    /* Update the cut window if necessary */

    redraw_all_cut_canvases( );
}


/*-------------------------------------------------------------------------*
 * Function for toggling 1D display between normal and sliding window mode
 *-------------------------------------------------------------------------*/

void
change_mode( long mode,
             long width )
{
    long curves;
    long i;
    Scaled_Point_T *sp;
    Marker_1d_T *m, *mn;
    Curve_1d_T *cv;


    if ( G.mode == mode )
    {
        print( WARN, "Display is already in \"%s\" mode.\n",
               G.mode ? "SLIDING WINDOW" : "NORMAL" );
        return;
    }

    /* Clear all curves and markers */

    for ( curves = 0; curves < G_1d.nc; curves++ )
    {
        cv = G_1d.curve[ curves ];

        if ( width != G_1d.nx )
        {
            cv->points = T_realloc( cv->points, width * sizeof *cv->points );
            cv->xpoints = T_realloc( cv->xpoints, width * sizeof *cv->xpoints );
        }

        for ( sp = cv->points, i = 0; i < width; sp++, i++ )
            sp->exist = UNSET;

        cv->can_undo = UNSET;
        cv->shift[ X ] = 0.0;
        cv->count = 0;
        cv->s2d[ X ] = ( double ) ( G_1d.canvas.w - 1 )
                       / ( double ) ( width - 1 );
    }

    for ( m = G_1d.marker_1d; m != NULL; m = mn )
    {
        XFreeGC( G.d, m->gc );
        mn = m->next;
        m = T_free( m );
    }

    G_1d.marker_1d = NULL;

    G.mode = mode;
    G_1d.nx = width;

    fl_set_form_title( GUI.run_form_1d->run_1d, mode == NORMAL_DISPLAY ?
                       "fsc2: 1D-Display" : 
                       "fsc2: 1D-Display (sliding window)" );

    redraw_all_1d( );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
