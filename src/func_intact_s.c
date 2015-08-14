/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
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


extern Toolbox_T *Toolbox;        /* defined in func_intact.c */


static Var_T *f_screate_child( Var_T          * v,
                               Iobject_Type_T   type,
                               double           start_val,
                               double           end_val,
                               double           step );
static void f_sdelete_child( Var_T * v );
static void f_sdelete_parent( Var_T * v );
static Var_T *f_svalue_child( Var_T * v );
static Var_T *f_schanged_child( Var_T * v );


/*----------------------------------------------------*
 * Function for appending a new slider to the toolbox
 *----------------------------------------------------*/

Var_T *
f_screate( Var_T * var )
{
    Var_T *v = var;
    Iobject_T *new_io = NULL;
    Iobject_T *ioi;
    volatile Iobject_Type_T type;
    volatile double start_val,
                    end_val;
    volatile double step = 0.0;
    char * volatile label = NULL;
    char * volatile help_text = NULL;


    CLOBBER_PROTECT( v );
    CLOBBER_PROTECT( new_io );

    /* We need at least the type of the slider and the minimum and maximum
       value */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be type of slider ("NORMAL_SLIDER", "VALUE_SLIDER"
       "SLOW_NORMAL_SLIDER" or "SLOW_VLAUE_SLIDER" or 0, 1, 2 or 3) */

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type == INT_VAR || v->type == FLOAT_VAR )
    {
        type = get_strict_long( v, "slider type" ) + FIRST_SLIDER_TYPE;

        if ( ! IS_SLIDER( type ) )
        {
            print( FATAL, "Invalid slider type (%ld).\n",
                   ( long ) ( type - FIRST_SLIDER_TYPE ) );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( ! strcasecmp( v->val.sptr, "NORMAL_SLIDER" ) )
            type = NORMAL_SLIDER;
        else if ( ! strcasecmp( v->val.sptr, "VALUE_SLIDER" ) )
            type = VALUE_SLIDER;
        else if ( ! strcasecmp( v->val.sptr, "SLOW_NORMAL_SLIDER" ) )
            type = SLOW_NORMAL_SLIDER;
        else if ( ! strcasecmp( v->val.sptr, "SLOW_VALUE_SLIDER" ) )
            type = SLOW_VALUE_SLIDER;
        else
        {
            print( FATAL, "Unknown slider type: '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }

    /* Get the minimum value for the slider */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing slider minimum value.\n" );
        THROW( EXCEPTION );
    }

    vars_check( v, INT_VAR | FLOAT_VAR );
    start_val = VALUE( v );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing slider maximum value.\n" );
        THROW( EXCEPTION );
    }

    /* Get the maximum value and make sure it's larger than the minimum */

    vars_check( v, INT_VAR | FLOAT_VAR );
    end_val = VALUE( v );

    if ( end_val <= start_val )
    {
        print( FATAL, "Maximum not larger than minimum slider value.\n" );
        THROW( EXCEPTION );
    }

    if ( v->next != NULL && v->next->type & ( INT_VAR | FLOAT_VAR ) )
    {
        v = vars_pop( v );
        vars_check( v, INT_VAR | FLOAT_VAR );
        step = fabs( VALUE( v ) );
        if ( step < 0.0 )
        {
            print( FATAL, "Negative slider step width.\n" );
            THROW( EXCEPTION );
        }

        if ( step > end_val - start_val )
        {
            print( FATAL, "Slider step size larger than difference between "
                   "minimum and maximum value.\n" );
            THROW( EXCEPTION );
        }
    }

    /* The child process has to pass the parameter to the parent and ask it to
       create the slider */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_screate_child( v, type, start_val, end_val, step );

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        vars_check( v, STR_VAR );

        if ( *v->val.sptr != '\0' )
        {
            TRY
            {
                label = T_strdup( v->val.sptr );
                check_label( label );
                convert_escapes( label );
                TRY_SUCCESS;
            }
            OTHERWISE
            {
                T_free( label );
                RETHROW;
            }
        }
    }

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        TRY
        {
            vars_check( v, STR_VAR );
            help_text = T_strdup( v->val.sptr );
            convert_escapes( help_text );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( help_text );
            T_free( label );
            RETHROW;
        }
    }

    too_many_arguments( v );

    /* Now that we're done with checking the parameters we can create the new
       slider - if the Toolbox doesn't exist yet we've got to create it now */

    TRY
    {
        if ( Toolbox == NULL )
            toolbox_create( VERT );

        new_io = T_malloc( sizeof *new_io );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( new_io );
        T_free( help_text );
        T_free( label );
        RETHROW;
    }

    if ( Toolbox->objs == NULL )
    {
        Toolbox->objs = new_io;
        new_io->next = new_io->prev = NULL;
    }
    else
    {
        for ( ioi = Toolbox->objs; ioi->next != NULL; ioi = ioi->next )
            /* empty */ ;
        ioi->next = new_io;
        new_io->prev = ioi;
        new_io->next = NULL;
    }

    new_io->ID = Toolbox->next_ID++;
    new_io->type = type;
    new_io->self = NULL;
    new_io->start_val = start_val;
    new_io->end_val = end_val;
    new_io->step = step;
    new_io->value = 0.5 * ( end_val + start_val );
    if ( step != 0.0 )
        new_io->value = lrnd( ( new_io->value - start_val ) / step ) * step
                        + start_val;
    new_io->label         = label;
    new_io->help_text     = help_text;
    new_io->is_changed    = UNSET;
    new_io->report_change = UNSET;
    new_io->enabled       = SET;
    new_io->partner       = -1;

    /* Draw the new slider */

    recreate_Toolbox( );

    return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------------------------------------------*
 * Part of the f_screate() function run by the child process only,
 * indirectly invoking the f_screate() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static Var_T *
f_screate_child( Var_T          * v,
                 Iobject_Type_T   type,
                 double           start_val,
                 double           end_val,
                 double           step )
{
    char *buffer,
         *pos;
    long new_ID;
    long *result;
    size_t len;
    char *label = NULL;
    char *help_text = NULL;


    if ( ( v = vars_pop( v ) ) != NULL )
    {
        vars_check( v, STR_VAR );
        label = v->val.sptr;

        if ( v->next != NULL )
        {
            vars_check( v->next, STR_VAR );
            help_text = v->next->val.sptr;

            if ( v->next->next != NULL )
                print( WARN, "Too many arguments, discarding superfluous "
                       "arguments.\n" );
        }
    }

    len =   sizeof EDL.Lc + sizeof type
          + sizeof start_val + sizeof end_val + sizeof step;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;
    if ( label )
        len += strlen( label ) + 1;
    else
        len++;
    if ( help_text )
        len += strlen( help_text ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* store current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &type, sizeof type );     /* store slider type */
    pos += sizeof type;

    memcpy( pos, &start_val, sizeof start_val );
    pos += sizeof start_val;

    memcpy( pos, &end_val, sizeof end_val );
    pos += sizeof end_val;

    memcpy( pos, &step, sizeof step );
    pos += sizeof step;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* store current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    if ( label )                            /* store label string */
    {
        strcpy( pos, label );
        pos += strlen( label ) + 1;
    }
    else
        *pos++ = '\0';

    if ( help_text )                        /* store help text string */
    {
        strcpy( pos, help_text );
        pos += strlen( help_text ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent to create the slider - it returns an array width two
       elements, the first indicating if it was successful, the second being
       the sliders ID */

    result = exp_screate( buffer, pos - buffer );

    /* Bomb out if parent returns failure */

    if ( result[ 0 ] == 0 )
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    new_ID = result[ 1 ];
    T_free( result );

    return vars_push( INT_VAR, new_ID );
}


/*-----------------------------------------------------------------------*
 * Deletes one or more sliders, parameter(s) are one or more slider IDs.
 *-----------------------------------------------------------------------*/

Var_T *
f_sdelete( Var_T * v )
{
    /* At least one slider ID is needed... */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Loop over all slider IDs - since the child has no control over the
       sliders, it has to ask the parent process to delete the slider */

    do
    {
        if ( Fsc2_Internals.I_am == CHILD )
            f_sdelete_child( v );
        else
            f_sdelete_parent( v );
    } while ( ( v = vars_pop( v ) ) != NULL );

    if (    Fsc2_Internals.I_am == CHILD
         || Fsc2_Internals.mode == TEST
         || ! Toolbox )
        return vars_push( INT_VAR, 1L );

    /* Redraw the tool box without the slider */

    recreate_Toolbox( );

    return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*
 * Part of the f_sdelete() function run by the child process only,
 * indirectly invoking the f_sdelete() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static void
f_sdelete_child( Var_T * v )
{
    char *buffer,
         *pos;
    size_t len;
    long ID;


    /* Very basic sanity check */

    ID = get_strict_long( v, "slider ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid slider identifier.\n" );
        THROW( EXCEPTION );
    }

    len = sizeof EDL.Lc + sizeof v->val.lval;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );          /* slider ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );        /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Bomb out on failure */

    if ( ! exp_sdelete( buffer, pos - buffer ) )
        THROW( EXCEPTION );
}


/*---------------------------------------------------------*
 * Part of the f_sdelete() function only run by the parent
 * process, which actually removes the slider.
 *---------------------------------------------------------*/

static void
f_sdelete_parent( Var_T * v )
{
    Iobject_T *io;


    /* No tool box -> no sliders to delete */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No sliders have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that slider with the ID exists */

    io = find_object_from_ID( get_strict_long( v, "slider ID" ) );

    if ( io == NULL || ! IS_SLIDER( io->type ) )
    {
        print( FATAL, "Invalid slider identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Remove slider from the linked list */

    if ( io->next != NULL )
        io->next->prev = io->prev;
    if ( io->prev != NULL )
        io->prev->next = io->next;
    else
        Toolbox->objs = io->next;

    /* Delete the slider object if its drawn */

    if ( Fsc2_Internals.mode != TEST && io->self )
    {
        fl_delete_object( io->self );
        fl_free_object( io->self );
    }

    T_free( ( void * ) io->label );
    T_free( ( void * ) io->help_text );
    T_free( io );

    /* If this was the very last object also delete the form */

    if ( Toolbox->objs == NULL )
    {
        toolbox_delete( );

        if ( v->next != NULL )
        {
            print( FATAL, "Invalid slider identifier.\n" );
            THROW( EXCEPTION );
        }
    }
}


/*--------------------------------------.-------------------*
 * Function for quering or setting the position of a slider
 *----------------------------------------------------------*/

Var_T *
f_svalue( Var_T * v )
{
    Iobject_T *io;


    /* We need at least the sliders ID */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child has to pass the arguments to the parent and ask it
       to set or return the slider value */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_svalue_child( v );

    /* No tool box -> no sliders... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No slider have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that ID is ID of a slider */

    io = find_object_from_ID( get_strict_long( v, "slider ID" ) );

    if ( io == NULL || ! IS_SLIDER( io->type ) )
    {
        print( FATAL, "Invalid slider identifier.\n" );
        THROW( EXCEPTION );
    }

    io->is_changed = UNSET;

    /* If there are no more arguments just return the sliders value */

    if ( ( v = vars_pop( v ) ) == NULL )
        return vars_push( FLOAT_VAR, io->value );

    /* Otherwise check the next argument, i.e. the value to be set - it
       must be within the sliders range, if it doesn't fit reduce it to
       the allowed range */

    vars_check( v, INT_VAR | FLOAT_VAR );

    io->value = VALUE( v );

    if ( io->value > io->end_val )
    {
        print( SEVERE, "Slider value too large.\n" );
        io->value = io->end_val;
    }

    if ( io->value < io->start_val )
    {
        print( SEVERE, "Slider value too small.\n" );
        io->value = io->start_val;
    }

    /* If a certain step width is used set the new value to the nearest
       allowed value */

    if ( io->step != 0.0 )
        io->value = lrnd( ( io->value - io->start_val ) / io->step )
                    * io->step + io->start_val;

    if ( Fsc2_Internals.mode != TEST )
        fl_set_slider_value( io->self, io->value );

    too_many_arguments( v );

    return vars_push( FLOAT_VAR, io->value );
}


/*----------------------------------------------------------------*
 * Part of the f_svalue() function run by the child process only,
 * indirectly invoking the f_svalue() function in the parent via
 * the message passing mechanism.
 *----------------------------------------------------------------*/

static Var_T *
f_svalue_child( Var_T * v )
{
    long ID;
    long state = 0;
    double val = 0.0;
    char *buffer,
         *pos;
    double *res;
    size_t len;


    /* Very basic sanity check... */

    ID = get_strict_long( v, "slider ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid slider identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Another arguments means that the slider value is to be set */

    if ( ( v = vars_pop( v ) ) != NULL )
    {
        vars_check( v, INT_VAR | FLOAT_VAR );
        state = 1;
        val = VALUE( v );
    }

    too_many_arguments( v );

    len = sizeof EDL.Lc + sizeof ID + sizeof state + sizeof val;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );             /* sliders ID */
    pos += sizeof ID;

    memcpy( pos, &state, sizeof state );       /* needs slider setting ? */
    pos += sizeof state;

    memcpy( pos, &val, sizeof val );           /* new slider value */
    pos += sizeof val;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );              /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent to set or get the slider value - it will return an array
       with two doubles, the first indicating if it was successful (when the
       value is positive) the second is the slider value */

    res = exp_sstate( buffer, pos - buffer );

    /* Bomb out on failure */

    if ( res[ 0 ] < 0.0 )
    {
        T_free( res );
        THROW( EXCEPTION );
    }

    val = res[ 1 ];
    T_free( res );

    return vars_push( FLOAT_VAR, val );
}


/*----------------------------------------------------------*
 * Function for testing if the position of a slider changed
 *----------------------------------------------------------*/

Var_T *
f_schanged( Var_T * v )
{
    Iobject_T *io;


    /* We need the sliders ID */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child has to pass the arguments to the parent and ask it
       if the slider value did change */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_schanged_child( v );

    /* No tool box -> no sliders... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No slider have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that ID is ID of a slider */

    io = find_object_from_ID( get_strict_long( v, "slider ID" ) );

    if ( io == NULL || ! IS_SLIDER( io->type ) )
    {
        print( FATAL, "Invalid slider identifier.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, ( long ) io->is_changed );
}


/*------------------------------------------------------------------*
 * Part of the f_schanged() function run by the child process only,
 * indirectly invoking the f_schanged() function in the parent via
 * the message passing mechanism.
 *------------------------------------------------------------------*/

static Var_T *
f_schanged_child( Var_T * v )
{
    long ID;
    char *buffer,
         *pos;
    long *res;
    long val;
    size_t len;


    /* Very basic sanity check... */

    ID = get_strict_long( v, "slider ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid slider identifier.\n" );
        THROW( EXCEPTION );
    }

    len = sizeof EDL.Lc + sizeof ID;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );             /* sliders ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );              /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent if the slider got changed - it will return an array with two
       longs, the first indicating if it was successful (when the value is
       positive) and the second indicated if the slider got changed. */

    res = exp_schanged( buffer, pos - buffer );

    /* Bomb out on failure */

    if ( res[ 0 ] < 0 )
    {
        T_free( res );
        THROW( EXCEPTION );
    }

    val = res[ 1 ];
    T_free( res );

    return vars_push( INT_VAR, val );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
