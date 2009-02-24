/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


extern Toolbox_T *Toolbox;        /* defined in func_intact.c */


static Var_T *f_ocreate_child( Var_T *        v,
                               Iobject_Type_T type,
                               long           lval,
                               double         dval,
                               char *         sptr );
static void f_odelete_child( Var_T * v );
static void f_odelete_parent( Var_T * v );
static Var_T *f_ovalue_child( Var_T * v );
static Var_T *f_ochanged_child( Var_T * v );


/*-----------------------------------------------------------*
 * For appending a new input or output object to the toolbox
 *-----------------------------------------------------------*/

Var_T *
f_ocreate( Var_T * var )
{
    Var_T *v = var;
    Iobject_Type_T type;
    char *label = NULL;
    char *help_text = NULL;
    char *form_str = NULL;
    Iobject_T *new_io = NULL;
    Iobject_T *ioi;
    long lval = 0;
    double dval = 0.0;
    char *sptr = NULL;


    CLOBBER_PROTECT( v );
    CLOBBER_PROTECT( type );
    CLOBBER_PROTECT( label );
    CLOBBER_PROTECT( help_text );
    CLOBBER_PROTECT( form_str );
    CLOBBER_PROTECT( new_io );
    CLOBBER_PROTECT( lval );
    CLOBBER_PROTECT( dval );
    CLOBBER_PROTECT( sptr );

    /* At least the type of the input or output object must be specified */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be type of object ("INT_INPUT", "FLOAT_INPUT",
       "INT_OUTPUT", "FLOAT_OUTPUT", "STRING_OUTPUT" or 0, 1, 2, 3, or 4) */

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type == INT_VAR || v->type == FLOAT_VAR )
    {
        type = get_strict_long( v, "input or output object type" )
               + FIRST_INOUTPUT_TYPE;

        if ( ! IS_INOUTPUT( type ) )
        {
            print( FATAL, "Invalid input or output object type (%ld).\n",
                   ( long ) ( type - FIRST_INOUTPUT_TYPE ) );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( ! strcasecmp( v->val.sptr, "INT_INPUT" ) )
            type = INT_INPUT;
        else if ( ! strcasecmp( v->val.sptr, "FLOAT_INPUT" ) )
            type = FLOAT_INPUT;
        else if ( ! strcasecmp( v->val.sptr, "INT_OUTPUT" ) )
            type = INT_OUTPUT;
        else if ( ! strcasecmp( v->val.sptr, "FLOAT_OUTPUT" ) )
            type = FLOAT_OUTPUT;
        else if ( ! strcasecmp( v->val.sptr, "STRING_OUTPUT" ) )
            type = STRING_OUTPUT;
        else
        {
            print( FATAL, "Unknown input or output object type: '%s'.\n",
                   v->val.sptr );
            THROW( EXCEPTION );
        }
    }

    /* Next argument could be a value to be set in the input or output
       object */

    if ( ( v = vars_pop( v ) ) != NULL ) 
    {
        switch ( v->type )
        {
            case INT_VAR :
                if ( type == INT_INPUT || type == INT_OUTPUT )
                    lval = v->val.lval;
                else if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
                    dval = VALUE( v );
                else
                    sptr = get_string( "%d", v->val.lval );
                v = vars_pop( v );
                break;

            case FLOAT_VAR :
                if ( type == INT_INPUT || type == INT_OUTPUT )
                {
                    print( SEVERE, "Float value used as initial value for new "
                           "integer input or output object.\n" );
                    lval = lrnd( v->val.dval );
                }
                else if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
                    dval = v->val.dval;
                else
                    sptr = get_string( "%f", v->val.dval );
                v = vars_pop( v );
                break;

            case STR_VAR :
                if ( type == STRING_OUTPUT )
                {
                    sptr = T_strdup( v->val.sptr );
                    v = vars_pop( v );
                }
                break;

            default:
                print( FATAL, "Invalid variable type for input or "
                       "output object.\n" );
                THROW( EXCEPTION );
        }
    } else if ( type == STRING_OUTPUT )
        sptr = T_strdup( "" );

    /* Since the child process can't use graphics it has to write the
       parameter into a buffer, pass it to the parent process and ask the
       parent to create the object (due to the possibly allocated memory for
       a string output field we need to call the function in a TRY block
       to be able to get rid of that memory in all cases). */

    if ( Fsc2_Internals.I_am == CHILD )
    {
        TRY
        {
            v = f_ocreate_child( v, type, lval, dval, sptr );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            if ( type == STRING_OUTPUT )
                T_free( sptr );
            RETHROW( );
        }

        if ( type == STRING_OUTPUT )
            T_free( sptr );
        return v;
    }

    /* Next argument is the label string */

    if ( v != NULL )
    {
        vars_check( v, STR_VAR );

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
            T_free( sptr );
            RETHROW( );
        }

        v = vars_pop( v );
    }

    /* Next argument can be a help text */

    if ( v != NULL )
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
            T_free( sptr );
            RETHROW( );
        }

        v = vars_pop( v );
    }

    /* Final argument can be a C format string */

    if ( v != NULL )
    {
        TRY
        {
            vars_check( v, STR_VAR );
            if (    type == INT_INPUT
                 || type == INT_OUTPUT
                 || type == STRING_OUTPUT )
                print( WARN, "Can't set format string for integer or "
                       "string data.\n" );
            else
            {
                form_str = T_strdup( v->val.sptr );
                if ( ! check_format_string( form_str ) )
                {
                    print( FATAL, "Invalid format string \"%s\".\n",
                           v->val.sptr );
                    THROW( EXCEPTION );
                }
            }
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( form_str );
            T_free( help_text );
            T_free( label );
            T_free( sptr );
            RETHROW( );
        }
    }

    /* Warn and get rid of superfluous arguments */

    too_many_arguments( v );

    /* Now that we're done with checking the parameters we can create the new
       input or output field - if the Toolbox doesn't exist yet we've got to
       create it now */

    TRY
    {
        if ( Toolbox == NULL )
            toolbox_create( VERT );

        new_io = T_malloc( sizeof *new_io );

        /* We already do the following here while we're in the TRY block... */

        new_io->form_str = NULL;
        if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
        {
            if ( form_str )
                new_io->form_str = T_strdup( form_str );
            else
                new_io->form_str = T_strdup( "%g" );
        }

        form_str = T_free( form_str );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( new_io )
        {
            if ( new_io->form_str )
                T_free( new_io->form_str );
            T_free( new_io );
        }
        T_free( help_text );
        T_free( label );
        T_free( sptr );
        RETHROW( );
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
    if ( type == INT_INPUT || type == INT_OUTPUT )
        new_io->val.lval = lval;
    else if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
        new_io->val.dval = dval;
    else
        new_io->val.sptr = sptr;

    new_io->self          = NULL;
    new_io->group         = NULL;
    new_io->label         = label;
    new_io->help_text     = help_text;
    new_io->is_changed    = UNSET;
    new_io->report_change = UNSET;
    new_io->enabled       = SET;
    new_io->partner       = -1;

    /* Draw the new object */

    recreate_Toolbox( );

    return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------------------------------------------*
 * Part of the f_ocreate() function run by the child process only,
 * indirectly invoking the f_ocreate() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static Var_T *
f_ocreate_child( Var_T *        v,
                 Iobject_Type_T type,
                 long           lval,
                 double         dval,
                 char *         sptr )
{
    char *buffer, *pos;
    long new_ID;
    long *result;
    size_t len;
    char *label = NULL;
    char *help_text = NULL;
    char *form_str = NULL;


    /* Next argument is the label string */

    if ( v != NULL )
    {
        vars_check( v, STR_VAR );
        label = v->val.sptr;

        /* Next argument can be a help text */
        
        if ( v->next != NULL )
        {
            vars_check( v->next, STR_VAR );
            help_text = v->next->val.sptr;

            /* Final argument can be a C format string */

            if ( v->next->next != NULL )
            {
                vars_check( v->next->next, STR_VAR );
                if (    type == INT_INPUT
                     || type == INT_OUTPUT
                     || type == STRING_OUTPUT )
                    print( WARN, "Can't set format string for integer "
                           "data.\n" );
                form_str = v->next->next->val.sptr;

                /* Warn if there are superfluous arguments */

                if ( v->next->next->next != NULL )
                    print( WARN, "Too many arguments, discarding "
                           "superfluous arguments.\n" );
            }
        }
    }

    /* Calculate length of buffer needed */

    len = sizeof EDL.Lc + sizeof type;

    if ( type == INT_INPUT || type == INT_OUTPUT )
        len += sizeof lval;
    else if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
        len +=sizeof dval;
    else
        len += strlen( sptr ) + 1;

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

    if ( form_str )
        len += strlen( form_str ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &type, sizeof type );
    pos += sizeof type;

    if ( type == INT_INPUT || type == INT_OUTPUT )
    {
        memcpy( pos, &lval, sizeof lval );
        pos += sizeof lval;
    }
    else if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
    {
        memcpy( pos, &dval, sizeof dval );
        pos += sizeof dval;
    }
    else
    {
        strcpy( pos, sptr );
        pos += strlen( sptr ) + 1;
    }

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    if ( label )                            /* label string */
    {
        strcpy( pos, label );
        pos += strlen( label ) + 1;
    }
    else
        *pos++ = '\0';

    if ( help_text )                        /* help text string */
    {
        if ( *help_text == '\0' )
        {
            * ( unsigned char * ) pos = 0xff;
            pos += 1;
        }
        else
        {
            strcpy( pos, help_text );
            pos += strlen( help_text ) + 1;
        }
    }
    else
        *pos++ = '\0';

    if ( form_str )
    {
        strcpy( pos, form_str );
        pos += strlen( form_str ) + 1;
    }
    else
        *pos++ = '\0';

    /* Pass buffer to parent and ask it to create the input or input
       object. It returns a buffer with two longs, the first one indicating
       success or failure (1 or 0), the second being the objects ID */

    result = exp_icreate( buffer, pos - buffer );

    if ( result[ 0 ] == 0 )      /* failure -> bomb out */
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    new_ID = result[ 1 ];
    T_free( result );           /* free result buffer */

    return vars_push( INT_VAR, new_ID );
}


/*----------------------------------------------*
 * Deletes one or more input or output objects.
 * Parameters are one or more object IDs.
 *----------------------------------------------*/

Var_T *
f_odelete( Var_T * v )
{
    /* We need the ID of the input/output object to delete */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Loop over all variables - since the object 'belong' to the parent, the
       child needs to ask the parent to delete the object. The ID of each
       object to be deleted gets passed to the parent in a buffer and the
       parent is asked to delete the object */

    do
    {
        if ( Fsc2_Internals.I_am == CHILD )
            f_odelete_child( v );
        else
            f_odelete_parent( v );
    } while ( ( v = vars_pop( v ) ) != NULL );

    /* The child process is already done here, and in a test run we're also */

    if (    Fsc2_Internals.I_am == CHILD
         || Fsc2_Internals.mode == TEST
         || ! Toolbox )
        return vars_push( INT_VAR, 1L );

    /* Redraw the form without the deleted objects */

    recreate_Toolbox( );

    return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*
 * Part of the f_odelete() function run by the child process only,
 * indirectly invoking the f_odelete() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static void
f_odelete_child( Var_T * v )
{
    char *buffer, *pos;
    size_t len;
    long ID;


    /* Do all possible checks on the parameter */

    ID = get_strict_long( v, "input or output object ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid input or output object identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Get a bufer long enough and write data */

    len = sizeof EDL.Lc + sizeof v->val.lval;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );    /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );            /* object ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );             /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent to delete the object, bomb out on failure */

    if ( ! exp_idelete( buffer, pos - buffer ) )
        THROW( EXCEPTION );
}


/*------------------------------------------------------------------*
 * Part of the f_odelete() function only run by the parent process,
 * which actually removes the input or output object.
 *------------------------------------------------------------------*/

static void
f_odelete_parent( Var_T * v )
{
    Iobject_T *io;


    /* No tool box -> no objects -> no objects to delete... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No input or output objects have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Do checks on parameters */

    io = find_object_from_ID( get_strict_long( v,
                                               "input or output object ID" ) );

    if ( io == NULL || ! IS_INOUTPUT( io->type ) )
    {
        print( FATAL, "Invalid input or output object identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Remove object from the linked list */

    if ( io->next != NULL )
        io->next->prev = io->prev;
    if ( io->prev != NULL )
        io->prev->next = io->next;
    else
        Toolbox->objs = io->next;

    /* Delete the object (its not drawn in a test run!) */

    if ( Fsc2_Internals.mode != TEST )
    {
        fl_delete_object( io->self );
        fl_free_object( io->self );
    }

    if ( io->type == STRING_OUTPUT )
        T_free( io->val.sptr );

    T_free( io->label );
    T_free( io->help_text );
    if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
        T_free( io->form_str );
    T_free( io );

    if ( Toolbox->objs == NULL )
    {
        toolbox_delete( );

        if ( v->next != NULL )
        {
            print( FATAL, "Invalid input or output object identifier%s.\n",
                   v->next->next != NULL ? "s" : "" );
            THROW( EXCEPTION );
        }
    }
}


/*----------------------------------------------------------*
 * Sets or returns the content of an input or output object
 *----------------------------------------------------------*/

Var_T *
f_ovalue( Var_T * v )
{
    Iobject_T *io;
    char buf[ MAX_INPUT_CHARS + 1 ];


    /* We need at least the objects ID */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child has to pass the arguments to the parent and ask it
       to set or return the objects value */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_ovalue_child( v );

    /* No tool box -> no input or output objects... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No input or output objects have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that ID is ID of an input or output object */

    io = find_object_from_ID( get_strict_long( v,
                                               "input or output object ID" ) );

    if ( io == NULL || ! IS_INOUTPUT( io->type ) )
    {
        print( FATAL, "Invalid input or output object identifier.\n" );
        THROW( EXCEPTION );
    }

    io->is_changed = UNSET;

    /* If there are no more arguments just return the objects value */

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
            return vars_push( INT_VAR, io->val.lval );
        else if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
            return vars_push( FLOAT_VAR, io->val.dval );
        else
            return vars_push( STR_VAR, io->val.sptr );
    }

    /* Otherwise check the next argument, i.e. the value to be set */

    switch ( v->type )
    {
        case INT_VAR :
            if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
                io->val.lval = v->val.lval;
            else if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
                io->val.dval = VALUE( v );
            else
                io->val.sptr = get_string( "%d", v->val.lval );
            break;

        case FLOAT_VAR :
            if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
            {
                print( SEVERE, "Float value used as initial value for new "
                       "integer input or output object.\n" );
                io->val.lval = lrnd( v->val.dval );
            }
            else if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
                io->val.dval = v->val.dval;
            else
                io->val.sptr = get_string( "%f", v->val.dval );
            break;

        case STR_VAR :
            if ( io->type == STRING_OUTPUT )
            {
                T_free( io->val.sptr );
                io->val.sptr = T_strdup( v->val.sptr );
            }
            else
            {
                print( FATAL, "Can't use a string as the value for an "
                       "int or float input or output object.\n" );
                THROW( EXCEPTION );
            }
            break;

        default:
            print( FATAL, "Invalid variable type for input or "
                   "output object.\n" );
            THROW( EXCEPTION );
    }

    if ( Fsc2_Internals.mode != TEST )
    {
        if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
            snprintf( buf, MAX_INPUT_CHARS + 1, "%ld", io->val.lval );
        else if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
            snprintf( buf, MAX_INPUT_CHARS + 1, io->form_str, io->val.dval );
        else
            snprintf( buf, MAX_INPUT_CHARS + 1, "%s", io->val.sptr );

        fl_set_input( io->self, buf );
    }

    too_many_arguments( v );

    if ( io->type == INT_INPUT || io->type == INT_OUTPUT )
        return vars_push( INT_VAR, io->val.lval );
    else if ( io->type == FLOAT_INPUT || io->type == FLOAT_OUTPUT )
        return vars_push( FLOAT_VAR, io->val.dval );
    else
        return vars_push( STR_VAR, io->val.sptr );
}


/*----------------------------------------------------------------*
 * Part of the f_ovalue() function run by the child process only,
 * indirectly invoking the f_ovalue() function in the parent via
 * the message passing mechanism.
 *----------------------------------------------------------------*/

static Var_T *
f_ovalue_child( Var_T * v )
{
    long ID;
    long state = 0;
    long lval = 0;
    double dval = 0.0;
    char *sptr = NULL;
    char *buffer, *pos;
    Input_Res_T *input_res;
    size_t len;


    /* Very basic sanity check... */

    ID = get_strict_long( v, "input or output object ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid input or output object identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Another argument means that the objects value is to be set */

    if ( ( v = vars_pop( v ) ) != NULL )
        switch ( v->type )
        {
            case INT_VAR :
                state = INT_VAR;
                lval = v->val.lval;
                break;

            case FLOAT_VAR :
                state = FLOAT_VAR;
                dval = VALUE( v );
                break;

            case STR_VAR :
                state = STR_VAR;
                sptr = T_strdup( v->val.sptr );
                break;

            default :
                print( FATAL, "Invalid type to set for the input or output "
                       "object.\n" );
                THROW( EXCEPTION );
        }

    too_many_arguments( v );

    len = sizeof EDL.Lc + sizeof ID + sizeof state;
    if ( state == 0 || state == INT_VAR )
        len += sizeof lval;
    else if ( state == FLOAT_VAR )
        len += sizeof dval;
    else if ( state == STR_VAR )
        len += strlen( sptr ) + 1;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );          /* object ID */
    pos += sizeof ID;

    memcpy( pos, &state, sizeof state );      /* needs input setting ? */
    pos += sizeof state;

    if ( state == 0 || state == INT_VAR )     /* new object value */
    {
        memcpy( pos, &lval, sizeof lval );
        pos += sizeof lval;
    }
    else if ( state == FLOAT_VAR )
    {
        memcpy( pos, &dval, sizeof dval );
        pos += sizeof dval;
    }
    else if ( state == STR_VAR )
    {
        strcpy( pos, sptr );
        pos += strlen( sptr ) + 1;
        T_free( sptr );
    }

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent to set or get the value - it will return a pointer to an
       Input_Res structure, where the res entry indicates failure (negative
       value) and the state of the returned value and the second entry is a
       union for the return value, i.e. the objects value or a pointer to
       a string. */

    input_res = exp_istate( buffer, pos - buffer );

    /* Bomb out on failure */

    if ( input_res->res < 0 )
    {
        T_free( input_res );
        THROW( EXCEPTION );
    }

    state = input_res->res;
    if ( state == INT_VAR )
        lval = input_res->val.lval;
    else if ( state == FLOAT_VAR )
        dval = input_res->val.dval;
    else if ( state == STR_VAR )
        sptr = input_res->val.sptr;

    T_free( input_res );

    if ( state == INT_VAR )
        return vars_push( INT_VAR, lval );
    else if ( state == FLOAT_VAR )
        return vars_push( FLOAT_VAR, dval );

    v = vars_push( STR_VAR, sptr );
    T_free( sptr );
    return v;
}


/*--------------------------------------------------------------------------*
 * Function for testing if the content of an input or output object changed
 *--------------------------------------------------------------------------*/

Var_T *
f_ochanged( Var_T * v )
{
    Iobject_T *io;


    /* We need at least the objects ID */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* As usual the child has to pass the arguments to the parent and ask it
       to return if the objects got changed */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_ochanged_child( v );

    /* No tool box -> no input or output objects... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No input or output objects have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that the ID is one of an input or output object */

    io = find_object_from_ID( get_strict_long( v,
                                               "input or output object ID" ) );

    if ( io == NULL || ! IS_INOUTPUT( io->type ) )
    {
        print( FATAL, "Invalid input or output object identifier.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, ( long ) io->is_changed );
}


/*------------------------------------------------------------------*
 * Part of the f_ochanged() function run by the child process only,
 * indirectly invoking the f_ochanged() function in the parent via
 * the message passing mechanism.
 *------------------------------------------------------------------*/

static Var_T *
f_ochanged_child( Var_T * v )
{
    long ID;
    long changed;
    char *buffer, *pos;
    long *res;
    size_t len;


    /* Very basic sanity check... */

    ID = get_strict_long( v, "input or output object ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid input or output object identifier.\n" );
        THROW( EXCEPTION );
    }

    len = sizeof EDL.Lc + sizeof ID + 1;

    if ( EDL.Fname )
        len += strlen( EDL.Fname );

    pos = buffer = T_malloc( len );

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );          /* input or output object ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    res = exp_ichanged( buffer, pos - buffer );

    /* Bomb out on failure */

    if ( res[ 0 ] < 0 )
    {
        T_free( res );
        THROW( EXCEPTION );
    }

    changed = res[ 1 ];
    T_free( res );

    return vars_push( INT_VAR, changed );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
