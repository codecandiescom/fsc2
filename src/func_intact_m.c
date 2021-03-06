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


static Var_T * f_mcreate_child( Var_T  * v,
                                size_t   len,
                                long     num_strs );
static void f_madd_child( Var_T * v );
static void f_madd_parent( Var_T * v );
static Var_T * f_mtext_child( Var_T *v );
static Var_T * f_mtext_parent( Var_T *v );
static void f_mdelete_child( Var_T * v );
static void f_mdelete_parent( Var_T * v );
static Var_T * f_mchoice_child( Var_T * v );
static Var_T * f_mchanged_child( Var_T * v );


/*---------------------------------------------------------*
 * Function for appending a new menu object to the toolbox
 *---------------------------------------------------------*/

Var_T *
f_mcreate( Var_T * volatile var )
{
    /* At least a label and one menu entry must be specified */

    Var_T * volatile v = var;
    if ( v == NULL || v->next == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    size_t len = 0;
    volatile long num_strs = 0;
    for ( Var_T * lv = v; lv != NULL; num_strs++, lv = lv->next )
    {
        if ( lv->type != STR_VAR )
        {
            print( FATAL, "All arguments must be strings.\n" );
            THROW( EXCEPTION );
        }

        len += strlen( lv->val.sptr ) + 1;
    }

    if ( Fsc2_Internals.I_am == CHILD )
        return f_mcreate_child( v, len, num_strs );

    /* Now that we're done with checking the parameters we can create the new
       menu - if the Toolbox doesn't exist yet we've got to create it now */

    if ( Toolbox == NULL )
        toolbox_create( VERT );

    Iobject_T * volatile new_io = NULL;
    Iobject_T * volatile ioi = NULL;
    TRY
    {
        new_io = T_malloc( sizeof *new_io );
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

        new_io->ID            = Toolbox->next_ID++;
        new_io->type          = MENU;
        new_io->self          = NULL;
        new_io->state         = 1;
        new_io->is_changed    = false;
        new_io->report_change = false;
        new_io->enabled       = true;
        new_io->partner       = -1;
        new_io->label         = NULL;
        new_io->menu_items    = NULL;
        new_io->label         = T_strdup( v->val.sptr );

        v = vars_pop( v );

        check_label( new_io->label );
        convert_escapes( new_io->label );

        new_io->help_text = NULL;

        new_io->num_items = num_strs - 1;
        new_io->menu_items = T_malloc(   new_io->num_items
                                       * sizeof *new_io->menu_items );

        for  ( long i = 0; i < new_io->num_items; i++ )
            new_io->menu_items[ i ] = NULL;
        for ( long i = 0; v && i < new_io->num_items; i++, v = vars_pop( v ) )
            new_io->menu_items[ i ] = T_strdup( v->val.sptr );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        if ( new_io != NULL )
        {
            T_free( new_io->label );
            if ( new_io->menu_items != NULL )
            {
                for ( long i = 0; i < new_io->num_items; i++ )
                    T_free( new_io->menu_items[ i ] );
                T_free( new_io->menu_items );
            }

            if( Toolbox->objs == new_io )
                Toolbox->objs = NULL;
            else
                ioi->next = NULL;

            T_free( new_io );
        }

        RETHROW;
    }

    /* Draw the new menu */

    recreate_Toolbox( );

    return vars_push( INT_VAR, new_io->ID );
}


/*-----------------------------------------------------------------*
 * Part of the f_mcreate() function run by the child process only,
 * indirectly invoking the f_mcreate() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static
Var_T *
f_mcreate_child( Var_T  * v,
                 size_t   len,
                 long     num_strs )
{
    len += sizeof EDL.Lc + sizeof num_strs;
    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );     /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &num_strs, sizeof num_strs );
    pos += sizeof num_strs;

    if ( EDL.Fname )
    {
        size_t l = strlen( EDL.Fname ) + 1;
        memcpy( pos, EDL.Fname, l );           /* current file name */
        pos += l;
    }
    else
        *pos++ = '\0';

    for ( ; v != NULL; v = v->next )
    {
        size_t l = strlen( v->val.sptr ) + 1;
        memcpy( pos, v->val.sptr, l );
        pos += l;
    }

    /* Pass buffer to parent and ask it to create the menu object. It returns
       a buffer with two longs, the first one indicating success or failure (1
       or 0), the second being the new objects ID */

    long * result = exp_mcreate( buffer, pos - buffer );

    if ( result[ 0 ] == 0 )      /* failure -> bomb out */
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    long new_ID = result[ 1 ];
    T_free( result );           /* free result buffer */

    return vars_push( INT_VAR, new_ID );
}


/*--------------------------------------------------------*
 * Function for adding one or more lines to a menu object
 *--------------------------------------------------------*/

Var_T *
f_madd( Var_T * v )
{
    /* At least a menu ID and a string to be added is needed... */

    if ( v == NULL || v->next == NULL)
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    if ( Fsc2_Internals.I_am == CHILD )
        f_madd_child( v );
    else
        f_madd_parent( v );

    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static
void
f_madd_child( Var_T * v )
{
    /* Very basic sanity check */

    long ID = get_strict_long( v, "menu ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    long add_count = 0;
    size_t len = sizeof EDL.Lc + sizeof v->val.lval + sizeof add_count;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    for ( Var_T * ev = v = vars_pop( v ); ev; ev = ev->next, add_count++ )
    {
        if ( ev->type != STR_VAR )
        {
            print( FATAL, "Invalid argument, new entries to be added must "
                   "be strings.\n" );
            THROW( EXCEPTION );
        }

        len += strlen( ev->val.sptr ) + 1;
    }

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );      /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );              /* menu ID */
    pos += sizeof ID;

    memcpy( pos, &add_count, sizeof add_count );   /* number of strings */
    pos += sizeof add_count;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );               /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    for ( ; v != NULL; v = v->next )
    {
        size_t l = strlen( v->val.sptr ) + 1;
        memcpy( pos, v->val.sptr, l );
        pos += l;
    }

    /* Bomb out on failure */

    if ( ! exp_madd( buffer, pos - buffer ) )
        THROW( EXCEPTION );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static
void
f_madd_parent( Var_T * v )
{
    /* No tool box -> no menu we could add entries to */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No menus have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that menu with the ID exists */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

    if ( io == NULL || io->type != MENU )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    long add_count = 0;
    for ( Var_T  * ev = v = vars_pop( v ); ev; add_count++, ev = ev->next )
        if ( ev->type != STR_VAR )
        {
            print( FATAL, "Invalid argument, new entries to be added must "
                   "be strings.\n" );
            THROW( EXCEPTION );
        }

    io->num_items += add_count;
    io->menu_items = T_realloc( io->menu_items,
                                io->num_items * sizeof *io->menu_items );

    for  ( long i = io->num_items - add_count; i < io->num_items; i++ )
        io->menu_items[ i ] = NULL;

    for ( long i = io->num_items - add_count; v && i < io->num_items;
          i++, v = vars_pop( v ) )
    {
        io->menu_items[ i ] = T_strdup( v->val.sptr );
        if (    Fsc2_Internals.mode != TEST
             && fl_addto_choice( io->self, io->menu_items[ i ] ) == 0 )
        {
            print( FATAL, "Adding entry to menu failed.\n" );
            THROW( EXCEPTION );
        }
    }
}


/*-------------------------------------------------------------*
 * Function for changing the text of an entry of a menu object
 *-------------------------------------------------------------*/

Var_T *
f_mtext( Var_T * v )
{
    /* At least a menu ID and an item number are needed... */

    if ( v == NULL || v->next == NULL)
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* If there's another argument it must be a non-empty string */

    if ( v->next->next != NULL )
    {
        if ( ! ( v->next->next->type & STR_VAR ) )
        {
            print( FATAL, "Invald third argument, must be a string.\n" );
            THROW( EXCEPTION );
        }
        else if ( *v->next->next->val.sptr == '\0' )
        {
            print( FATAL, "Can't set menu item text to an empty string.\n" );
            THROW( EXCEPTION );
        }
    }

    if ( Fsc2_Internals.I_am == CHILD )
        return f_mtext_child( v );
    else
        return f_mtext_parent( v );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static
Var_T *
f_mtext_child( Var_T * v )
{
    /* Basic check of menu identifier - always the first parameter */

    long ID = get_strict_long( v, "menu ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    /* Get and rudimentarily check the second required argument */

    long item = get_strict_long( v, "menu item number" );

    if ( item <= 0 )
    {
        print( FATAL, "Invalid menu item number %ld.\n", item );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    size_t len = sizeof EDL.Lc + 2 * sizeof( long );

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    if ( v != NULL )
        len += strlen( v->val.sptr ) + 1;
    else
        len++;

    char *buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );      /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );              /* menu ID */
    pos += sizeof ID;

    memcpy( pos, &item, sizeof item );          /* item number */
    pos += sizeof item;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );               /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    if ( v != NULL )
    {
        strcpy( pos, v->val.sptr );             /* items text */
        pos += strlen( v->val.sptr ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent process to return or set the menu items text - bomb out if
       it returns NULL, indicating a severe error */

    char * result = exp_mtext( buffer, pos - buffer );

    if ( result == NULL )                       /* failure -> bomb out */
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    v = vars_push( STR_VAR, result );
    T_free( result );           /* free result buffer */

    return v;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

static
Var_T *
f_mtext_parent( Var_T *v )
{
    /* No tool box -> no menu we could add entries to */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No menus have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that menu with the ID exists */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

    if ( io == NULL || io->type != MENU )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    /* Get the item number and check for validity */

    long item = get_strict_long( v, "menu item number" );

    if ( item <= 0 )
    {
        print( FATAL, "Invalid menu item number %ld.\n", item );
        THROW( EXCEPTION );
    }

    if ( item > io->num_items )
    {
        print( FATAL, "Invalid menu item number %ld, there are only %ld "
               "items.\n", item, io->num_items );
        THROW( EXCEPTION );
    }

    v = vars_pop( v );

    if ( v != NULL )
    {
        io->menu_items[ item - 1 ] = T_free( io->menu_items[ item - 1 ] );
        io->menu_items[ item - 1 ] = T_strdup( v->val.sptr );

        if ( Fsc2_Internals.mode != TEST )
            fl_replace_choice( io->self, item, v->val.sptr );
    }

    too_many_arguments( v );

    return vars_push( STR_VAR, io->menu_items[ item - 1 ] );
}


/*--------------------------------------------------------------------------*
 * Deletes one or more menu objects, parameter(s) are one or more menu IDs.
 *--------------------------------------------------------------------------*/

Var_T *
f_mdelete( Var_T * v )
{
    /* At least one menu ID is needed... */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Loop over all menu IDs - since the child has no control over the menu
       it has to ask the parent process to delete the menu */

    do
    {
        if ( Fsc2_Internals.I_am == CHILD )
            f_mdelete_child( v );
        else
            f_mdelete_parent( v );
    } while ( ( v = vars_pop( v ) ) != NULL );

    if (    Fsc2_Internals.I_am == CHILD
         || Fsc2_Internals.mode == TEST
         || ! Toolbox )
        return vars_push( INT_VAR, 1L );

    /* Redraw the tool box without the menu */

    recreate_Toolbox( );

    return vars_push( INT_VAR, 1L );
}


/*-----------------------------------------------------------------*
 * Part of the f_mdelete() function run by the child process only,
 * indirectly invoking the f_mdelete() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static
void
f_mdelete_child( Var_T * v )
{
    /* Very basic sanity check */

    long ID = get_strict_long( v, "menu ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    size_t len = sizeof EDL.Lc + sizeof v->val.lval;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );      /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );              /* menu ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );               /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Bomb out on failure */

    if ( ! exp_mdelete( buffer, pos - buffer ) )
        THROW( EXCEPTION );
}


/*---------------------------------------------------------*
 * Part of the f_mdelete() function only run by the parent
 * process, which actually removes the menu.
 *---------------------------------------------------------*/

static
void
f_mdelete_parent( Var_T * v )
{
    /* No tool box -> no menu to delete */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No menus have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check that menu with the ID exists */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

    if ( io == NULL || io->type != MENU )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Remove menu from the linked list */

    if ( io->next != NULL )
        io->next->prev = io->prev;
    if ( io->prev != NULL )
        io->prev->next = io->next;
    else
        Toolbox->objs = io->next;

    /* Delete the menu object if its drawn */

    if ( Fsc2_Internals.mode != TEST && io->self )
    {
        fl_delete_object( io->self );
        fl_free_object( io->self );
    }

    T_free( ( void * ) io->label );
    for ( long i = 0; i < io->num_items; i++ )
        T_free( ( void * ) io->menu_items[ i ] );
    T_free( io->menu_items );
    T_free( io );

    /* If this was the very last object delete also the form */

    if ( Toolbox->objs == NULL )
    {
        toolbox_delete( );

        if ( v->next != NULL )
        {
            print( FATAL, "Invalid menu identifier.\n" );
            THROW( EXCEPTION );
        }
    }
}


/*----------------------------------------------------*
 * Sets or returns the selected item in a menu object
 *----------------------------------------------------*/

Var_T *
f_mchoice( Var_T * v )
{
    /* We need at least the ID of the menu */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child doesn't know about the menu, so it got to ask the
       parent process */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_mchoice_child( v );

    /* No tool box -> no menus -> no menu item to set or get... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No menus have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check the menu ID parameter */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

    if ( io == NULL || io->type != MENU )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    io->is_changed = false;

    /* If there's no second parameter just return the currently selected menu
       item */

    if ( ( v = vars_pop( v ) ) == NULL )
        return vars_push( INT_VAR, ( long ) io->state );

    /* The optional second argument is the menu item to be set */

    long select_item  = get_strict_long( v, "menu item number" );

    /* A number less than 1 can only happen during the testing stage and
       we bomb out */

    if ( select_item <= 0 )
    {
        print( FATAL, "Invalid menu item number %ld.\n", select_item );
        THROW( EXCEPTION );
    }

    /* If the menu item number is larger than the total mumber of items
       we better bomb out (during the experiment we deal with this by just
       returning the currently selected choice) */

    if ( select_item > io->num_items )
    {
        if ( Fsc2_Internals.mode == TEST )
        {
            print( FATAL, "Invalid menu item number %ld, there are only %ld "
                   "items.\n", io->num_items );
            THROW( EXCEPTION );
        }

        print( SEVERE, "Invalid menu item number %ld, there are only %ld "
               "items.\n", io->num_items );
        return vars_push( INT_VAR, ( long ) io->state );
    }

    /* If this isn't a test run set the new menu item */

    io->state = select_item;
    if ( Fsc2_Internals.mode != TEST )
        fl_set_choice( io->self, select_item );

    too_many_arguments( v );

    return vars_push( INT_VAR, select_item );
}


/*-----------------------------------------------------------------*
 * Part of the f_mchoice() function run by the child process only,
 * indirectly invoking the f_mchoice() function in the parent via
 * the message passing mechanism.
 *-----------------------------------------------------------------*/

static
Var_T *
f_mchoice_child( Var_T * v )
{
    /* Basic check of menu identifier - always the first parameter */

    long ID = get_strict_long( v, "menu ID" );
    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    /* If there's a second parameter it's the item in the menu to set */

    long select_item = 0;
    if ( ( v = vars_pop( v ) ) != NULL )
    {
        select_item = get_strict_long( v, "menu item number" );

        /* All menu item numbers must be larger than 0, but since we're
           already running the experiment we don't bomb out but return the
           currently selected item instead */

        if ( select_item <= 0 )
        {
            print( SEVERE, "Invalid menu item number %ld.\n", select_item );
            select_item = 0;
        }
    }

    /* No more parameters accepted... */

    too_many_arguments( v );

    /* Make up buffer to send to parent process */

    size_t len = sizeof EDL.Lc + sizeof ID + sizeof select_item;

    if ( EDL.Fname )
        len += strlen( EDL.Fname ) + 1;
    else
        len++;

    char * buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );          /* menu ID */
    pos += sizeof ID;

    memcpy( pos, &select_item, sizeof select_item );
                                                /* menu item to be set (or */
    pos += sizeof select_item;                  /* 0 if not to be set)     */

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent process to return or set the menu item - bomb out if it
       returns a non-positive value, indicating a severe error */

    long * result = exp_mchoice( buffer, pos - buffer );

    if ( result[ 0 ] == 0 )      /* failure -> bomb out */
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    select_item = result[ 1 ];
    T_free( result );           /* free result buffer */

    return vars_push( INT_VAR, select_item );
}


/*-------------------------------------------------------*
 * Returns if the selected item of a menu object changed
 *-------------------------------------------------------*/

Var_T *
f_mchanged( Var_T * v )
{
    /* We need at least the ID of the menu */

    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    /* Again, the child doesn't know about the menu, so it got to ask the
       parent process */

    if ( Fsc2_Internals.I_am == CHILD )
        return f_mchanged_child( v );

    /* No tool box -> no menus -> no menu item to set or get... */

    if ( Toolbox == NULL || Toolbox->objs == NULL )
    {
        print( FATAL, "No menus have been defined yet.\n" );
        THROW( EXCEPTION );
    }

    /* Check the menu ID parameter */

    Iobject_T * io = find_object_from_ID( get_strict_long( v, "menu ID" ) );

    if ( io == NULL || io->type != MENU )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    return vars_push( INT_VAR, ( long ) io->is_changed );
}


/*------------------------------------------------------------------*
 * Part of the f_mchanged() function run by the child process only,
 * indirectly invoking the f_mAchanged() function in the parent via
 * the message passing mechanism.
 *------------------------------------------------------------------*/

static
Var_T *
f_mchanged_child( Var_T * v )
{
    /* Basic check of menu identifier - always the first parameter */

    long ID = get_strict_long( v, "menu ID" );

    if ( ID < ID_OFFSET )
    {
        print( FATAL, "Invalid menu identifier.\n" );
        THROW( EXCEPTION );
    }

    /* Make up buffer to send to parent process */

    size_t len = sizeof EDL.Lc + sizeof ID + 1;

    if ( EDL.Fname )
        len += strlen( EDL.Fname );

    char *buffer = T_malloc( len );
    char * pos = buffer;

    memcpy( pos, &EDL.Lc, sizeof EDL.Lc );  /* current line number */
    pos += sizeof EDL.Lc;

    memcpy( pos, &ID, sizeof ID );          /* menu ID */
    pos += sizeof ID;

    if ( EDL.Fname )
    {
        strcpy( pos, EDL.Fname );           /* current file name */
        pos += strlen( EDL.Fname ) + 1;
    }
    else
        *pos++ = '\0';

    /* Ask parent process if the selected item changed - bomb out if it
       returns a non-positive value, indicating a severe error */

    long * result = exp_mchanged( buffer, pos - buffer );

    if ( result[ 0 ] == 0 )      /* failure -> bomb out */
    {
        T_free( result );
        THROW( EXCEPTION );
    }

    long changed = result[ 1 ];
    T_free( result );           /* free result buffer */

    return vars_push( INT_VAR, changed );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
