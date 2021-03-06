/*
 *  Copyright (C) 1999-2016 Jens Thoms Toerring
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


static Var_T *f_bcreate_child( Var_T          * v,
                               Iobject_Type_T   type,
                               long             coll );
static void f_bdelete_child( Var_T * v );
static void f_bdelete_parent( Var_T * v );
static Var_T *f_bstate_child( Var_T * v );
static Var_T *f_bchanged_child( Var_T * v );


/*----------------------------------------------------*
 * Function for appending a new button to the toolbox
 *----------------------------------------------------*/

Var_T *
f_bcreate( Var_T * var )
{
    /* At least the type of the button must be specified */

    Var_T * volatile v = var;
    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* First argument must be type of button ("NORMAL_BUTTON", "PUSH_BUTTON"
       or "RADIO_BUTTON" or 0, 1, 2) */

    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    Iobject_Type_T volatile type;
    if ( v->type == INT_VAR || v->type == FLOAT_VAR )
    {
        type = get_strict_long( v, "button type" ) + FIRST_BUTTON_TYPE;

        if (    type != NORMAL_BUTTON
             && type != PUSH_BUTTON
             && type != RADIO_BUTTON )
        {
            print( FATAL, "Invalid button type (%ld).\n",
                   ( long ) ( type - FIRST_BUTTON_TYPE ) );
            THROW( EXCEPTION );
        }
    }
    else
    {
        if ( ! strcasecmp( v->val.sptr, "NORMAL_BUTTON" ) )
            type = NORMAL_BUTTON;
        else if ( ! strcasecmp( v->val.sptr, "PUSH_BUTTON" ) )
            type = PUSH_BUTTON;
        else if ( ! strcasecmp( v->val.sptr, "RADIO_BUTTON" ) )
            type = RADIO_BUTTON;
        else
        {
            print( FATAL, "Unknown button type: '%s'.\n", v->val.sptr );
            THROW( EXCEPTION );
        }
    }

    v = vars_pop( v );

    /* For radio buttons the next argument is the ID of the first button
       belonging to the group (except for the first button itself) */

    long volatile coll = -1;
    if ( type == RADIO_BUTTON )
    {
        if ( v != NULL && v->type & ( INT_VAR | FLOAT_VAR ) )
        {
            coll = get_strict_long( v, "radio button group leader ID" );

            /* Checking that the referenced group leader button exists
               and is also a RADIO_BUTTON can only be done by the parent */

            if ( Fsc2_Internals.I_am == PARENT )
            {
                /* Check that other group member exists at all */

                Iobject_T * cio = find_object_from_ID( coll );
                if ( ! cio )
                {
                    print( FATAL, "Specified group leader button does not "
                           "exist.\n", coll );
                    THROW( EXCEPTION );
                }

                /* The group leader must also be radio buttons */

                if ( cio->type != RADIO_BUTTON )
                {
                    print( FATAL, "Button specified as group leader isn't a "
                           "RADIO_BUTTON.\n", coll );
                    THROW( EXCEPTION );
                }
            }

            v = vars_pop( v );
        }
    }

    /* Since the child process can't use graphics it has to pass the parameter
       to the parent process and ask it to to create the button */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_bcreate_child( v, type, coll );

    /* Next argument got to be the label string */

    char * volatile label = NULL;
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
            RETHROW;
        }

        v = vars_pop( v );
    }

    /* Final argument can be a help text */

    char * volatile help_text = NULL;
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
            RETHROW;
        }

        v = vars_pop( v );
    }

    /* Warn and get rid of superfluous arguments */

    too_many_arguments( v );

    /* Now that we're done with checking the parameters we can create the new
       button - if the Toolbox doesn't exist yet we've got to create it now */

    Iobject_T * volatile new_io = NULL;
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
        Iobject_T * ioi;
        for ( ioi = Toolbox->objs; ioi->next != NULL; ioi = ioi->next )
            /* empty */ ;
        ioi->next    = new_io;
        new_io->prev = ioi;
        new_io->next = NULL;
    }

    new_io->ID            = Toolbox->next_ID++;
    new_io->type          = type;
    new_io->self          = NULL;
    new_io->group         = NULL;
    new_io->label         = label;
    new_io->help_text     = help_text;
    new_io->partner       = coll;
    new_io->is_changed    = false;
    new_io->report_change = false;
    new_io->enabled       = true;

    if ( type == RADIO_BUTTON && coll == -1 )
        new_io->state = 1;
    else
        new_io->state = 0;

    /* Draw the new button */

    recreate_Toolbox( );

    return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------------------------------------------*
 * Part of the f_bcreate() function run by the child process only,
 * indirectly invoking the f_bcreate() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static
Var_T *
f_bcreate_child( Var_T          * v,
                 Iobject_Type_T   type,
                 long             coll )
{
    /* We already got the type and the 'colleague' buttons number, the next
       argument is the label string */

    char * label = NULL;
    char * help_text = NULL;
    if ( v != NULL )
    {
        vars_check( v, STR_VAR );
        label = v->val.sptr;

        /* Final argument can be a help text */

        if ( v->next != NULL )
        {
            vars_check( v->next, STR_VAR );
            help_text = v->next->val.sptr;

            /* Warn and get rid of superfluous arguments */

            if ( v->next->next != NULL )
                print( WARN, "Too many arguments, discarding superfluous "
                       "arguments.\n" );
        }
    }

    /* Calculate length of buffer needed */

    size_t len = sizeof EDL.Lc + sizeof type + sizeof coll;

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

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &type, sizeof type );
    pos += sizeof type;

    memcpy( pos, &coll, sizeof coll );     /* group leaders ID */
    pos += sizeof coll;

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
        strcpy( pos, help_text );
        pos += strlen( help_text ) + 1;
    }
    else
        *pos++ = '\0';

    /* Pass buffer to the parent and ask it to create the button. It returns a
       buffer with two longs, the first indicating success or failure (value
       is 1 or 0), the second being the buttons ID */

    long * result = exp_bcreate( buffer, pos - buffer );

    if ( result[ 0 ] == 0 )      /* failure -> bomb out */
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    long new_ID = result[ 1 ];
    T_free( result );           /* free result buffer */

    return vars_push( INT_VAR, new_ID );
}


/*-----------------------------------------------------------------------*
 * Deletes one or more buttons, parameter(s) are one or more button IDs.
 *-----------------------------------------------------------------------*/

Var_T *
f_bdelete( Var_T * v )
{
    /* We need the ID of the button to delete */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Loop over all button numbers - s^ince the buttons 'belong' to the
       parent, the child needs to ask the parent to delete the button. The ID
       of each button to be deleted gets passed to the parent and the parent
       is asked to delete the button. */

    do
    {
        if ( Fsc2_Internals.I_am == CHILD )
            f_bdelete_child( v );
        else
            f_bdelete_parent( v );
    } while ( ( v = vars_pop( v ) ) != NULL );

    /* The child process is already done here and also a test run (or when the
       tool box is already deleted) */

    if (    Fsc2_Internals.I_am == CHILD
         || Fsc2_Internals.mode == TEST
         || ! Toolbox )
        return vars_push( INT_VAR, 1L );

    /* Redraw the form without the deleted buttons */

    recreate_Toolbox( );

    return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*
 * Part of the f_bdelete() function run by the child process only,
 * indirectly invoking the f_bdelete() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static
void
f_bdelete_child( Var_T * v )
{
    /* Do all possible checking of the parameter */

    long ID = get_strict_long( v, "button ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid button identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Get a long enough buffer and write data */

    size_t len = sizeof EDL.Lc + sizeof v->val.lval;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );   /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );           /* button ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );             /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent to delete the button, bomb out on failure */

    if ( ! exp_bdelete( buffer, pos - buffer ) )
        THROW( EXCEPTION );
}


/*---------------------------------------------------------*
 * Part of the f_bdelete() function only run by the parent
 * process, which actually removes the button.
 *---------------------------------------------------------*/

static
void
f_bdelete_parent( Var_T * v )
{
    /* No tool box -> no buttons -> no buttons to delete... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No buttons have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check the parameters */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "button ID" ) );

    if ( io == NULL || ! IS_BUTTON( io->type ) )
    {
        print( FATAL, "Invalid button identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Remove button from the linked list */

    if ( io->next != NULL )
        io->next->prev = io->prev;
    if ( io->prev != NULL )
        io->prev->next = io->next;
    else
        Toolbox->objs = io->next;

    /* Delete the button (it's not drawn in a test run!) */

    if ( Fsc2_Internals.mode != TEST )
    {
        fl_delete_object( io->self );
        fl_free_object( io->self );
    }

    /* Special care has to be taken for the first radio buttons of a group
       (i.e. the one the others refer to) is deleted - the next button from
       the group must be made group leader and the remaining buttons must get
       told about it) */

    if ( io->type == RADIO_BUTTON && io->partner == -1 )
    {
        long new_anchor = 0;
        Iobject_T * nio;

        for ( nio = io->next; nio != NULL; nio = nio->next )
            if ( nio->type == RADIO_BUTTON && nio->partner == io->ID )
            {
                new_anchor = nio->ID;
                nio->partner = -1;
                break;
            }

        if ( nio != NULL )
            for ( nio = nio->next; nio != NULL; nio = nio->next )
                if (    nio->type == RADIO_BUTTON
                     && nio->partner == io->ID )
                    nio->partner = new_anchor;
    }

    T_free( ( void * ) io->label );
    T_free( ( void * ) io->help_text );
    T_free( io );

    /* If this was the last object also delete the form */

    if ( Toolbox->objs == NULL )
    {
        toolbox_delete( );

        if ( v->next != NULL )
        {
            print( FATAL, "Invalid button identifier.\n" );
            THROW( EXCEPTION );
        }
    }
}


/*---------------------------------------*
 * Sets or returns the state of a button
 *---------------------------------------*/

Var_T *
f_bstate( Var_T * v )
{
    /* We need at least the ID of the button */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child doesn't know about the button, so it got to ask the
       parent process */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_bstate_child( v );

    /* No tool box -> no buttons -> no button state to set or get... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No buttons have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check the button ID parameter */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "button ID" ) );

    if ( io == NULL || ! IS_BUTTON( io->type ) )
    {
        print( FATAL, "Invalid button identifier.\n" );
        THROW( EXCEPTION );
    }

    io->is_changed = false;

    /* If there's no second parameter just return the button state - for
       NORMAL_BUTTONs return the number it was pressed since the last call
       and reset the counter to zero */

    long state;
    if ( ( v = vars_pop( v ) ) == NULL )
    {
        if ( io->type == NORMAL_BUTTON )
        {
            state = io->state;
            io->state = 0;
            return vars_push( INT_VAR, state );
        }

        return vars_push( INT_VAR, io->state != 0 ? 1L : 0L );
    }

    /* The optional second argument is the state to be set - but take care,
       the state of NORMAL_BUTTONs can't be set */

    if ( io->type == NORMAL_BUTTON )
    {
        print( FATAL, "Can't set state of a NORMAL_BUTTON.\n" );
        THROW( EXCEPTION );
    }

    state = get_boolean( v );

    /* Can't switch off a radio button that is switched on */

    if ( io->type == RADIO_BUTTON && state == 0 && io->state != 0 )
    {
        print( FATAL, "Can't switch off a RADIO_BUTTON.\n" );
        THROW( EXCEPTION );
    }

    io->state = state;

    /* If this isn't a test run set the button state */

    if ( Fsc2_Internals.mode != TEST )
        fl_set_button( io->self, io->state );

    /* If one of the radio buttons is set all the other buttons belonging
       to the group must become unset */

    if ( io->type == RADIO_BUTTON && io->state == 1 )
        for ( Iobject_T * oio = Toolbox->objs; oio != NULL; oio = oio->next )
        {
            if (    oio == io
                 || oio->type != RADIO_BUTTON
                 || oio->group != io->group )
                continue;

            oio->state = 0;
            if ( Fsc2_Internals.mode != TEST )
                fl_set_button( oio->self, oio->state );
        }

    too_many_arguments( v );

    return vars_push( INT_VAR, ( long ) io->state );
}


/*----------------------------------------------------------------*
 * Part of the f_bstate() function run by the child process only,
 * indirectly invoking the f_bstate() function in the parent via
 * the message passing mechanism.
 *----------------------------------------------------------------*/

static
Var_T *
f_bstate_child( Var_T * v )
{
    /* Basic check of button identifier - always the first parameter */

    long ID = get_strict_long( v, "button ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid button identifier.\n" );
        THROW( EXCEPTION );
    }

    /* If there's a second parameter it's the state of button to set */

    long chld_state = -1;
    if ( ( v = vars_pop( v ) ) )
        chld_state = get_boolean( v );

    /* No more parameters accepted... */

    too_many_arguments( v );

    /* Make up buffer to send to parent process */

    size_t len = sizeof EDL.Lc + sizeof ID + sizeof chld_state;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );     /* buttons ID */
    pos += sizeof ID;

    memcpy( pos, &chld_state, sizeof chld_state );
                                                /* state to be set (negative */
    pos += sizeof chld_state;                   /* if not to be set)         */

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent process to return or set the state */

    long * result = exp_bstate( buffer, pos - buffer );

    /* Bomb out if parent returns failure */

    if ( result[ 0 ] == 0 )
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    chld_state = result[ 1 ];
    T_free( result );

    return vars_push( INT_VAR, chld_state );
}


/*-------------------------------------------------------*
 * Function for testing if the state of a button changed
 *-------------------------------------------------------*/

Var_T *
f_bchanged( Var_T * v )
{
    /* We need at least the ID of the button */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child doesn't know about the button, so it got to ask the
       parent process */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_bchanged_child( v );

    /* No tool box -> no buttons -> no button state change possible... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No buttons have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check the button ID parameter */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "button ID" ) );

    if ( io == NULL || ! IS_BUTTON( io->type ) )
    {
        print( FATAL, "Invalid button identifier.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, ( long ) io->is_changed );
}


/*------------------------------------------------------------------*
 * Part of the f_bchanged() function run by the child process only,
 * indirectly invoking the f_bchanged() function in the parent via
 * the message passing mechanism.
 *------------------------------------------------------------------*/

static
Var_T *
f_bchanged_child( Var_T * v )
{
    /* Basic check of button identifier - always the first parameter */

    long ID = get_strict_long( v, "button ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid button identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Make up buffer to send to parent process */

    size_t len = sizeof EDL.Lc + sizeof ID + 1;
    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;

    char *buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc ); /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );     /* buttons ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent process to return the state change indicator */

    long * result = exp_bchanged( buffer, pos - buffer );

    /* Bomb out if parent returns failure */

    if ( result[ 0 ] == 0 )
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    int chld_changed = result[ 1 ];
    T_free( result );

    return vars_push( INT_VAR, chld_changed );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
