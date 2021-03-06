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

static const char *handle_input( const char * content,
                                 const char * label );


/*--------------------------------------------------------*
 * Shows a message box with an "OK" button - use embedded
 * newline characters to get multi-line messages.
 *--------------------------------------------------------*/

void
show_message( const char * str )
{
    if ( Fsc2_Internals.I_am == PARENT )
    {
        if (    Fsc2_Internals.cmdline_flags & DO_CHECK
             || Fsc2_Internals.cmdline_flags &
                                      ( TEST_ONLY | NO_GUI_RUN | BATCH_MODE ) )
        {
            fprintf( stdout, "%s\n", str );
        }
        else
        {
            switch_off_special_cursors( );
            Fsc2_Internals.state = STATE_WAITING;
            fl_show_messages( str );
            Fsc2_Internals.state = STATE_RUNNING;
        }
    }
    else
    {
        if ( ! writer( C_SHOW_MESSAGE, str ) || ! reader( NULL ) )
            THROW( EXCEPTION );
    }
}


/*-------------------------------------------------------*
 * Shows an alert box with an "OK" button - use embedded
 * newline characters to get multi-line messages.
 *-------------------------------------------------------*/

void
show_alert( const char * str )
{
    if ( Fsc2_Internals.I_am == PARENT )
    {
        char *strs[ 3 ];

        char * strc = T_strdup( str );
        strs[ 0 ] = strc;
        if ( ( strs[ 1 ] = strchr( strs[ 0 ], '\n' ) ) != NULL )
        {
            *strs[ 1 ]++ = '\0';
            if ( ( strs[ 2 ] = strchr( strs[ 1 ], '\n' ) ) != NULL )
                *strs[ 2 ]++ = '\0';
            else
                strs[ 2 ] = NULL;
        }
        else
        {
            strs[ 1 ] = strs[ 2 ] = NULL;
        }

        if (    Fsc2_Internals.cmdline_flags & DO_CHECK
             || Fsc2_Internals.cmdline_flags &
                                      ( TEST_ONLY | NO_GUI_RUN | BATCH_MODE ) )
        {
            for ( int i = 0; i < 3 && strs[ i ] != NULL; i++ )
                fprintf( stdout, "%s", strs[ i ] );
            fprintf( stdout, "\n" );
        }
        else
        {
            switch_off_special_cursors( );
            Fsc2_Internals.state = STATE_WAITING;
            fl_show_alert( strs[ 0 ], strs[ 1 ], strs[ 2 ], 1 );
            Fsc2_Internals.state = STATE_RUNNING;
        }

        T_free( strc );
    }
    else
    {
        if ( ! writer( C_SHOW_ALERT, str ) || ! reader( NULL ) )
            THROW( EXCEPTION );
    }
}


/*------------------------------------------------------------------------*
 * Shows a choice box with up to three buttons, returns the number of the
 * pressed button (in the range from 1 - 3).
 * ->
 *    1. message text, use embedded newlines for multi-line messages
 *    2. number of buttons to show
 *    3. texts for the three buttons
 *    4. number of button to be used as default button (range 1 - 3)
 *------------------------------------------------------------------------*/

int
show_choices( const char * text,
              int          numb,
              const char * b1,
              const char * b2,
              const char * b3,
              int          def,
              bool         is_batch )
{
    if ( Fsc2_Internals.I_am == PARENT )
    {
        if (    Fsc2_Internals.cmdline_flags & DO_CHECK
             || ( Fsc2_Internals.cmdline_flags & BATCH_MODE && is_batch )
             || ( Fsc2_Internals.cmdline_flags & ( TEST_ONLY | NO_GUI_RUN ) ) )
        {
            fprintf( stdout, "%s\n%s %s %s\n", text, b1, b2, b3 );
            return def != 0 ? def : 1;
        }
        else
        {
            switch_off_special_cursors( );
            Fsc2_Internals.state = STATE_WAITING;
            int ret = fl_show_choices( text, numb, b1, b2, b3, def );
            Fsc2_Internals.state = STATE_RUNNING;
            return ret;
        }
    }
    else
    {
        int ret;
        if (    ! writer( C_SHOW_CHOICES, text, numb, b1, b2, b3, def )
             || ! reader( &ret ) )
            THROW( EXCEPTION );
        return ret;
    }
}


/*---------------------------------------------------------------*
 * Shows a file selector box and returns the selected file name.
 * ->
 *    1. short message to be shown
 *    2. name of directory the file might be in
 *    3. pattern for files to be listed (e.g. "*.edl")
 *    4. default file name
 * <-
 * Returns either a static buffer with the file name or NULL.
 *---------------------------------------------------------------*/

const char *
show_fselector( const char * message,
                const char * directory,
                const char * pattern,
                const char * def )
{
    const char *ret = NULL;

    if ( Fsc2_Internals.I_am == PARENT )
    {
        switch_off_special_cursors( );
        Fsc2_Internals.state = STATE_WAITING;
        ret = fsc2_show_fselector( message, directory, pattern, def );
        Fsc2_Internals.state = STATE_RUNNING;
        return ret;
    }
    else
    {
        if (    ! writer( C_SHOW_FSELECTOR, message, directory, pattern, def )
             || ! reader( &ret ) )
            THROW( EXCEPTION );
        return ret;
    }
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

const char *
show_input( const char * content,
            const char * label )
{

    if ( Fsc2_Internals.I_am == PARENT )
    {
        switch_off_special_cursors( );
        return handle_input( content, label );
    }
    else
    {
        char *ret = NULL;

        if (    ! writer( C_INPUT, content, label )
             || ! reader( &ret ) )
            THROW( EXCEPTION );
        return ret;
    }
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

static const char *
handle_input( const char * content,
              const char * label )
{
    if ( label != NULL && label != '\0' )
        fl_set_object_label( GUI.input_form->comm_input, label );
    else
        fl_set_object_label( GUI.input_form->comm_input,
                             "Enter your comment:" );

    fl_set_input( GUI.input_form->comm_input, content );

    fl_show_form( GUI.input_form->input_form,
                  FL_PLACE_MOUSE | FL_FREE_SIZE, FL_FULLBORDER,
                  "fsc2: Comment editor" );

    Fsc2_Internals.state = STATE_WAITING;

    while ( fl_do_forms( ) != GUI.input_form->comm_done )
        /* empty */ ;

    Fsc2_Internals.state = STATE_RUNNING;

    if ( fl_form_is_visible( GUI.input_form->input_form ) )
        fl_hide_form( GUI.input_form->input_form );

    return fl_get_input( GUI.input_form->comm_input );
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the layout() function.
 *--------------------------------------------------------------*/

bool
exp_layout( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_LAYOUT, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char * old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get variable with address of function to set the layout */

            Var_T * func_ptr = func_get( "layout", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long type;
            memcpy( &type, pos, sizeof type );        /* get layout type */
            vars_push( INT_VAR, type );
            pos += sizeof type;

            EDL.Fname = pos;                          /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_LAYOUT_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_LAYOUT_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return true;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the button_create() function.
 *--------------------------------------------------------------*/

long *
exp_bcreate( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_BCREATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );

        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get variable with address of function to create a button */

            Var_T * func_ptr = func_get( "button_create", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            Iobject_Type_T type;
            memcpy( &type, pos, sizeof type );
            vars_push( INT_VAR, ( long ) type - FIRST_BUTTON_TYPE );
            pos += sizeof type;

            long val;
            memcpy( &val, pos, sizeof val );         /* get colleague ID */
            if ( val >= 0 )
                vars_push( INT_VAR, val );
            pos += sizeof val;

            EDL.Fname = pos;                         /* current file name */
            pos += strlen( pos ) + 1;

            vars_push( STR_VAR, pos );               /* get label string */
            pos += strlen( pos ) + 1;

            if ( *pos != '\0' )                      /* get help text */
                vars_push( STR_VAR, pos );

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_BCREATE_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );

        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the button_delete() function.
 *--------------------------------------------------------------*/

bool
exp_bdelete( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_BDELETE, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get variable with address of function to delete a button */

            Var_T * func_ptr = func_get( "button_delete", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );  /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                        /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_BDELETE_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_BDELETE_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the button_state() function .
 *--------------------------------------------------------------*/

long *
exp_bstate( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_BSTATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get variable with address of function to get a button state */

            Var_T * func_ptr = func_get( "button_state", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            long val;
            memcpy( &val, pos, sizeof val );         /* get state to be set */
            if ( val >= 0 )
                vars_push( INT_VAR, val );
            pos += sizeof val;

            EDL.Fname = pos;                         /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_BSTATE_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the button_changed() function .
 *--------------------------------------------------------------*/

long *
exp_bchanged( char      * buffer,
              ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_BCHANGED, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }

        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get variable with address of function to determine a button
               state change */

            Var_T * func_ptr = func_get( "button_changed", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                         /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_BCHANGED_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the slider_create() function .
 *--------------------------------------------------------------*/

long *
exp_screate( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_SCREATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get variable with address of function to create a slider */

            Var_T * func_ptr = func_get( "slider_create", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            Iobject_Type_T type;
            memcpy( &type, pos, sizeof type );
            vars_push( INT_VAR, ( long ) type - FIRST_SLIDER_TYPE );
            pos += sizeof type;

            for ( int i = 0; i < 3; i++ )
            {
                double val;
                memcpy( &val, pos, sizeof val );
                vars_push( FLOAT_VAR, val );
                pos += sizeof val;
            }

            EDL.Fname = pos;                          /* current file name */
            pos += strlen( pos ) + 1;

            vars_push( STR_VAR, pos );               /* get label string */
            pos += strlen( pos ) + 1;

            if ( *pos != '\0' )                      /* get help text */
                vars_push( STR_VAR, pos );

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_SCREATE_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the slider_delete() function.
 *--------------------------------------------------------------*/

bool
 exp_sdelete( char      * buffer,
              ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_SDELETE, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get variable with address of function to delete a slider */

            Var_T * func_ptr = func_get( "slider_delete", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_SDELETE_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_SDELETE_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the slider_value() function
 *--------------------------------------------------------------*/

double *
exp_sstate( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_SSTATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        double * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = -1.0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        double res[ 2 ];

        TRY
        {
            /* Get variable with address of function to set/get slider value */

            Var_T * func_ptr = func_get( "slider_value", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            long val;
            memcpy( &val, pos, sizeof val );
            pos += sizeof val;

            if ( val > 0 )
            {
                double dval;

                memcpy( &dval, pos, sizeof dval );
                vars_push( FLOAT_VAR, dval );
            }
            pos += sizeof( double );

            EDL.Fname = pos;                       /* get current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            res[ 0 ] = 1.0;
            res[ 1 ] = ret->val.dval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            res[ 0 ] = -1.0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_SSTATE_REPLY, sizeof res, res ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the slider_changed() function
 *--------------------------------------------------------------*/

long *
exp_schanged( char      * buffer,
              ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_SCHANGED, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = -1;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long res[ 2 ];

        TRY
        {
            /* Get variable with address of function to determine state
               changes */

            Var_T * func_ptr = func_get( "slider_changed", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                       /* get current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            res[ 0 ] = 1;
            res[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            res[ 0 ] = -1;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_SCHANGED_REPLY, sizeof res, res ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the input_create() function.
 *--------------------------------------------------------------*/

long *
exp_icreate( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_ICREATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );

        long * result = T_malloc( 2 * sizeof( long ) );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get function to create an input object */

            Var_T * func_ptr = func_get( "input_create", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            Iobject_Type_T type;
            memcpy( &type, pos, sizeof type );       /* type of input object */
            vars_push( INT_VAR, ( long ) type - FIRST_INOUTPUT_TYPE );
            pos += sizeof type;

            if ( type == INT_INPUT || type == INT_OUTPUT )
            {
                long lval;

                memcpy( &lval, pos, sizeof lval );
                vars_push( INT_VAR, lval );
                pos += sizeof lval;
            }
            else if ( type == FLOAT_INPUT || type == FLOAT_OUTPUT )
            {
                double dval;

                memcpy( &dval, pos, sizeof dval );
                vars_push( FLOAT_VAR, dval );
                pos += sizeof dval;
            }
            else
            {
                vars_push( STR_VAR, pos );
                pos += strlen( pos ) + 1;
            }

            EDL.Fname = pos;                         /* current file name */
            pos += strlen( pos ) + 1;

            vars_push( STR_VAR, pos );               /* get label string */
            pos += strlen( pos ) + 1;

            if ( *pos != '\0' )                      /* get help text */
            {
                if ( * ( ( unsigned char * ) pos ) == 0xff )
                {
                    vars_push( STR_VAR, "" );
                    pos += 1;
                }
                else
                {
                    vars_push( STR_VAR, pos );
                    pos += strlen( pos ) + 1;
                }
            }
            else
            {
                pos++;
            }

            if ( *pos != '\0' )                      /* get C format string */
                vars_push( STR_VAR, pos );

            /* Call the function */

            Var_T *ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_ICREATE_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the input_delete() function.
 *--------------------------------------------------------------*/

bool
exp_idelete( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_IDELETE, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function to delete an input object */

            Var_T * func_ptr = func_get( "input_delete", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_IDELETE_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_IDELETE_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the input_value() function
 *--------------------------------------------------------------*/

Input_Res_T *
exp_istate( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        /* Pass the data to the parent */

        if ( ! writer( C_ISTATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );

        /* Now get the parents reply */

        Input_Res_T * input_res = T_malloc( sizeof *input_res );
        if ( ! reader( input_res ) )
            input_res->res = -1;
        return input_res;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        Input_Res_T input_res = { -1, { 0 } };

        TRY
        {
            /* Get variable with address of function to set/get an input
               or output object value */

            Var_T * func_ptr = func_get( "input_value", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );            /* get object ID */
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            long type;
            memcpy( &type, pos, sizeof type );
            pos += sizeof type;

            if ( type == INT_VAR )                    /* new integer value */
            {
                long lval;

                memcpy( &lval, pos, sizeof lval );
                vars_push( INT_VAR, lval );
                pos += sizeof lval;
            }
            else if ( type == FLOAT_VAR )             /* new float value */
            {
                double dval;

                memcpy( &dval, pos, sizeof dval );
                vars_push( FLOAT_VAR, dval );
                pos += sizeof dval;
            }
            else if ( type == STR_VAR )                /* new string value */
            {
                vars_push( STR_VAR, pos );
                pos += strlen( pos ) + 1;
            }

            EDL.Fname = pos;                           /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );

            switch ( ret->type )
            {
                case INT_VAR :
                    input_res.res = INT_VAR;
                    input_res.val.lval = ret->val.lval;
                    break;

                case FLOAT_VAR :
                    input_res.res = FLOAT_VAR;
                    input_res.val.dval = ret->val.dval;
                    break;

                case STR_VAR :
                    input_res.res = STR_VAR;
                    input_res.val.sptr = T_strdup( ret->val.sptr );
                    break;

                default :
                    fsc2_impossible( );             /* this can't happen */
            }

            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            input_res.res = -1;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;

        if ( input_res.res != STR_VAR )
        {
            if ( ! writer( C_ISTATE_REPLY, sizeof input_res, &input_res ) )
                THROW( EXCEPTION );
        }
        else
        {
            if ( ! writer( C_ISTATE_STR_REPLY, strlen( input_res.val.sptr ),
                           input_res.val.sptr ) )
            {
                T_free( input_res.val.sptr );
                THROW( EXCEPTION );
            }

            T_free( input_res.val.sptr );
        }

        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing and returning the
 * arguments and results of the input_changed() function
 *--------------------------------------------------------------*/

long *
exp_ichanged( char      * buffer,
              ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_ICHANGED, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * res = T_malloc( 2 * sizeof *res );
        if ( ! reader( res ) )
            res[ 0 ] = -1;
        return res;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long res[ 2 ];

        TRY
        {
            /* Get variable with address of function to call */

            Var_T * func_ptr = func_get( "input_changed", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );            /* object ID */
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            res[ 0 ] = 0;
            res[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            res[ 0 ] = -1;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_ICHANGED_REPLY, sizeof res, res ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the menu_create() function.
 *--------------------------------------------------------------*/

long *
exp_mcreate( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_MCREATE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get function to create an menu object */

            Var_T * func_ptr = func_get( "menu_create", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long num_strs;
            memcpy( &num_strs, pos, sizeof num_strs );
            pos += sizeof num_strs;

            EDL.Fname = pos;                         /* current file name */
            pos += strlen( pos ) + 1;

            for ( long i = 0; i < num_strs; i++ )
            {
                vars_push( STR_VAR, pos );
                pos += strlen( pos ) + 1;
            }

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_MCREATE_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the menu_add() function.
 *--------------------------------------------------------------*/

bool
exp_madd( char      * buffer,
          ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_MADD, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function to add entries to a menu object */

            Var_T * func_ptr = func_get( "menu_add", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            pos += sizeof ID;
            vars_push( INT_VAR, ID );

            long num_strs;
            memcpy( &num_strs, pos, sizeof num_strs );
            pos += sizeof num_strs;

            EDL.Fname = pos;                         /* current file name */
            pos += strlen( pos ) + 1;

            for ( long i = 0; i < num_strs; i++ )
            {
                vars_push( STR_VAR, pos );
                pos += strlen( pos ) + 1;
            }

            vars_pop( func_call( func_ptr ) );
            writer( C_MADD_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_MADD_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return values of the menu_text() function.
 *--------------------------------------------------------------*/

char *
exp_mtext( char      * buffer,
           ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_MTEXT, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        char * ret;
        reader( &ret );
        return ret;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function to change an entry of a menu object */

            Var_T * func_ptr = func_get( "menu_text", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );           /* menu ID */
            pos += sizeof ID;
            vars_push( INT_VAR, ID );

            long item;
            memcpy( &item, pos, sizeof item );       /* entry ID */
            pos += sizeof item;
            vars_push( INT_VAR, item );

            EDL.Fname = pos;                         /* current file name */
            pos += strlen( pos ) + 1;

            if ( *pos != '\0' )                      /* empty string means */
                vars_push( STR_VAR, pos );           /* no third argument */

            Var_T * ret = func_call( func_ptr );
            writer( C_MTEXT_REPLY, ret->val.sptr );
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_MTEXT_REPLY, NULL );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the menu_delete() function.
 *--------------------------------------------------------------*/

bool
exp_mdelete( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_MDELETE, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *    old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get variable with address of function to delete a menu */

            Var_T * func_ptr = func_get( "menu_delete", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );            /* get menu ID */
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_MDELETE_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_MDELETE_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the menu_choice() function.
 *--------------------------------------------------------------*/

long *
exp_mchoice( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_MCHOICE, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get function for dealing with menu state */

            Var_T * func_ptr = func_get( "menu_choice", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            long val;
            memcpy( &val, pos, sizeof val );         /* menu item to be set */
            if ( val > 0 )
                vars_push( INT_VAR, val );
            pos += sizeof val;

            EDL.Fname = pos;                         /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_MCHOICE_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the menu_changed() function.
 *--------------------------------------------------------------*/

long *
exp_mchanged( char      * buffer,
              ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_MCHANGED, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 *     sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get function for dealing with menu state */

            Var_T * func_ptr = func_get( "menu_changed", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                         /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_MCHANGED_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the toolbox_changed() function.
 *--------------------------------------------------------------*/

long *
exp_tbchanged( char      * buffer,
               ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_TBCHANGED, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;
        long result[ 2 ];

        TRY
        {
            /* Get function for dealing with menu state */

            Var_T * func_ptr = func_get( "toolbox_changed", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            long var_count;
            memcpy( &var_count, pos, sizeof var_count );
            pos += sizeof var_count;

            while ( var_count-- )
            {
                long ID;
                memcpy( &ID, pos, sizeof ID );
                vars_push( INT_VAR, ID );
                pos += sizeof ID;
            }

            EDL.Fname = pos;                         /* current file name */

            /* Call the function */

            Var_T * ret = func_call( func_ptr );
            result[ 0 ] = 1;
            result[ 1 ] = ret->val.lval;
            vars_pop( ret );
            TRY_SUCCESS;
        }
        OTHERWISE
            result[ 0 ] = 0;

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        if ( ! writer( C_TBCHANGED_REPLY, sizeof result, result ) )
            THROW( EXCEPTION );
        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the toolbox_wait() function.
 *--------------------------------------------------------------*/

long *
exp_tbwait( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_TBWAIT, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        long * result = T_malloc( 2 * sizeof *result );
        if ( ! reader( result ) )
            result[ 0 ] = 0;
        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function for waiting for toolbox state change */

            Var_T * func_ptr = func_get( "toolbox_wait", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );   /* current line number */
            pos += sizeof EDL.Lc;

            double duration;
            memcpy( &duration, pos, sizeof duration );
            vars_push( FLOAT_VAR, duration );
            pos += sizeof duration;

            long var_count;
            memcpy( &var_count, pos, sizeof var_count );
            pos += sizeof var_count;

            while ( var_count-- )
            {
                long ID;
                memcpy( &ID, pos, sizeof ID );
                vars_push( INT_VAR, ID );
                pos += sizeof ID;
            }

            EDL.Fname = pos;                         /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );

            EDL.Fname = old_Fname;
            EDL.Lc = old_Lc;

            TRY_SUCCESS;
        }
        OTHERWISE
        {
            long result[ 2 ] = { 0, 0 };

            EDL.Fname = old_Fname;
            EDL.Lc = old_Lc;
            if ( ! writer( C_TBWAIT_REPLY, sizeof result, result ) )
                THROW( EXCEPTION );
        }

        return NULL;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the object_delete() function.
 *--------------------------------------------------------------*/

bool
exp_objdel( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_ODELETE, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function to delete an input object */

            Var_T * func_ptr = func_get( "object_delete", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );            /* get object ID */
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_ODELETE_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_ODELETE_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the object_change_label() function.
 *--------------------------------------------------------------*/

bool
 exp_clabel( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_CLABEL, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function to delete an input object */

            Var_T * func_ptr = func_get( "object_change_label", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );            /* get object ID */
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */
            pos += strlen( pos ) + 1;

            vars_push( STR_VAR, pos );                /* get label string */

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_CLABEL_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_CLABEL_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the object_enable() function.
 *--------------------------------------------------------------*/

bool
exp_xable( char      * buffer,
           ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_XABLE, len, buffer ) )
        {
            T_free( buffer );
            return false;
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        TRY
        {
            /* Get function to enable or disable an input object */

            Var_T * func_ptr = func_get( "object_enable", NULL );

            /* Unpack parameter and push them onto the stack */

            char * pos = buffer;
            memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
            pos += sizeof EDL.Lc;

            long ID;
            memcpy( &ID, pos, sizeof ID );            /* get object ID */
            vars_push( INT_VAR, ID );
            pos += sizeof ID;

            EDL.Fname = pos;                          /* current file name */
            pos += strlen( pos ) + 1;

            long state;
            memcpy( &state , pos, sizeof state );
            vars_push( INT_VAR, state );

            /* Call the function */

            vars_pop( func_call( func_ptr ) );
            writer( C_XABLE_REPLY, 1L );
            TRY_SUCCESS;
        }
        OTHERWISE
            writer( C_XABLE_REPLY, 0L );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
        return SET;
    }
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the mouse_position() function.
 *--------------------------------------------------------------*/

double *
exp_getpos( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_GETPOS, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }

        T_free( buffer );
        double * result = T_malloc( ( 2 * MAX_CURVES + 2 ) * sizeof *result );
        if ( ! reader( result ) )
        {
            T_free( result );
            THROW( EXCEPTION );
        }

        return result;
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        char * pos = buffer;
        long buttons;
        memcpy( &buttons, pos, sizeof buttons );    /* buttons to be handed */
        pos += sizeof buttons;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );      /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                            /* current file name */

        double result[ 2 * MAX_CURVES + 2 ];
        result[ 0 ] = 0;
        unsigned int keys;

        if ( buttons < 0 || G.button_state == buttons )
        {
            if ( G.focus & WINDOW_1D )
                result[ 0 ] = get_mouse_pos_1d( result + 1, &keys );
            else if ( G.focus & WINDOW_2D )
                result[ 0 ] = get_mouse_pos_2d( result + 1, &keys );
            else if ( G.focus & WINDOW_CUT )
                result[ 0 ] = get_mouse_pos_cut( result + 1, &keys );
        }

        if ( result[ 0 ] == 0.0 )
            for ( int i = 1; i < 2 * MAX_CURVES + 1; i++ )
                result[ i ] = 0.0;

        result[ 2 * MAX_CURVES + 1 ] = 0;
        if ( keys & ShiftMask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 0 );
        if ( keys & LockMask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 1 );
        if ( keys & ControlMask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 2 );
        if ( keys & Mod1Mask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 3 );
        if ( keys & Mod2Mask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 4 );
        if ( keys & Mod3Mask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 5 );
        if ( keys & Mod4Mask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 6 );
        if ( keys & Mod5Mask )
            result[ 2 * MAX_CURVES + 1 ] += ( 1 << 7 );

        writer( C_GETPOS_REPLY, sizeof result, result );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return NULL;
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the curve_button_1d() function.
 *--------------------------------------------------------------*/

bool
exp_cb_1d( char      * buffer,
           ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_CB_1D, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;


        char * pos = buffer;

        long button;
        memcpy( &button, pos, sizeof button );    /* button to be handled */
        pos += sizeof button;

        long state;
        memcpy( &state, pos, sizeof state );      /* what to do with button */
        pos += sizeof state;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                          /* current file name */

        FL_OBJECT *obj = NULL;
        switch ( button )
        {
            case 1 :
                obj = GUI.run_form_1d->curve_1_button_1d;
                break;

            case 2 :
                obj = GUI.run_form_1d->curve_2_button_1d;
                break;

            case 3 :
                obj = GUI.run_form_1d->curve_3_button_1d;
                break;

            case 4 :
                obj = GUI.run_form_1d->curve_4_button_1d;
                break;

            default :
                fsc2_impossible( );
        }

        long old_state = fl_get_button( obj );

        if ( state != -1 && old_state != state )
                curve_button_callback_1d( obj, button );

        writer( C_CB_1D_REPLY, old_state );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return SET;
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the curve_button_2d() function.
 *--------------------------------------------------------------*/

bool
exp_cb_2d( char      * buffer,
           ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_CB_2D, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        char * pos = buffer;

        long button;
        memcpy( &button, pos, sizeof button );    /* button to be handled */
        pos += sizeof button;

        long state;
        memcpy( &state, pos, sizeof state );      /* what to do with button */
        pos += sizeof state;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                          /* current file name */

        long old_state;
        if ( button == 0 )
        {
            old_state = 0;
            if ( fl_get_button( GUI.run_form_2d->curve_1_button_2d ) )
                old_state = 1;
            else if ( fl_get_button( GUI.run_form_2d->curve_2_button_2d ) )
                old_state = 2;
            else if ( fl_get_button( GUI.run_form_2d->curve_3_button_2d ) )
                old_state = 3;
            else if ( fl_get_button( GUI.run_form_2d->curve_4_button_2d ) )
                old_state = 4;
        }
        else
        {
            FL_OBJECT *obj = NULL;

            switch ( button )
            {
                case 1 :
                    obj = GUI.run_form_2d->curve_1_button_2d;
                    break;

                case 2 :
                    obj = GUI.run_form_2d->curve_2_button_2d;
                    break;

                case 3 :
                    obj = GUI.run_form_2d->curve_3_button_2d;
                    break;

                case 4 :
                    obj = GUI.run_form_2d->curve_4_button_2d;
                    break;

                default :
                    fsc2_impossible( );
            }

            old_state = fl_get_button( obj );

            if ( state != -1 && old_state != state )
                curve_button_callback_2d( obj, - button );
        }

        writer( C_CB_2D_REPLY, old_state );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return SET;
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the zoom_1d() function.
 *--------------------------------------------------------------*/

bool
exp_zoom_1d( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_ZOOM_1D, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        char * pos = buffer;

        double d[ 4 ];
        memcpy( d, pos, sizeof d );               /* new dimensions */
        pos += sizeof d;

        bool keep[ 4 ];
        memcpy( keep, pos, sizeof keep );         /* flags */
        pos += sizeof keep;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                          /* current file name */

        writer( C_ZOOM_1D_REPLY,
                ( long ) user_zoom_1d( d[ 0 ], keep[ 0 ],
                                       d[ 1 ], keep[ 1 ],
                                       d[ 2 ], keep[ 2 ],
                                       d[ 3 ], keep[ 3 ] ) );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return SET;
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the zoom_2d() function.
 *--------------------------------------------------------------*/

bool
exp_zoom_2d( char      * buffer,
             ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_ZOOM_2D, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        char * pos = buffer;

        long curve;
        memcpy( &curve, pos, sizeof curve );      /* curve to be zoomed */
        pos += sizeof curve;

        double d[ 6 ];
        memcpy( d, pos, sizeof d );               /* new dimensions */
        pos += sizeof d;

        bool keep[ 6 ];
        memcpy( keep, pos, sizeof keep );         /* flags */
        pos += sizeof keep;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                          /* current file name */

        writer( C_ZOOM_2D_REPLY,
                ( long ) user_zoom_2d( curve,
                                       d[ 0 ], keep[ 0 ],
                                       d[ 1 ], keep[ 1 ],
                                       d[ 2 ], keep[ 2 ],
                                       d[ 3 ], keep[ 3 ],
                                       d[ 4 ], keep[ 4 ],
                                       d[ 5 ], keep[ 5 ] ) );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return SET;
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the fs_button_1d() function.
 *--------------------------------------------------------------*/

bool
exp_fsb_1d( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_FSB_1D, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        char * pos = buffer;

        long state;
        memcpy( &state, pos, sizeof state );      /* what to do with button */
        pos += sizeof state;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                          /* current file name */

        FL_OBJECT * obj = GUI.run_form_1d->full_scale_button_1d;
        long old_state = fl_get_button( obj );

        if ( state != -1 && old_state != state )
        {
            fl_set_button( obj, state );
            fs_button_callback_1d( obj, state );
        }

        writer( C_FSB_1D_REPLY, old_state );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return SET;
}


/*--------------------------------------------------------------*
 * Child and parent side function for passing the arguments and
 * the return value of the fs_button_2d() function.
 *--------------------------------------------------------------*/

bool
exp_fsb_2d( char      * buffer,
            ptrdiff_t   len )
{
    if ( Fsc2_Internals.I_am == CHILD )
    {
        if ( ! writer( C_FSB_2D, len, buffer ) )
        {
            T_free( buffer );
            THROW( EXCEPTION );
        }
        T_free( buffer );
        return reader( NULL );
    }
    else
    {
        char *old_Fname = EDL.Fname;
        long old_Lc = EDL.Lc;

        char * pos = buffer;

        long curve;
        memcpy( &curve, pos, sizeof curve );      /* curve to be handled */
        pos += sizeof curve;

        long state;
        memcpy( &state, pos, sizeof state );      /* what to do with button */
        pos += sizeof state;

        memcpy( &EDL.Lc, pos, sizeof EDL.Lc );    /* current line number */
        pos += sizeof EDL.Lc;

        EDL.Fname = pos;                          /* current file name */

        long old_state;

        if ( G_2d.active_curve == curve - 1 )
        {
            FL_OBJECT *obj = GUI.run_form_2d->full_scale_button_2d;

            old_state = fl_get_button( obj );

            if ( state != -1 && old_state != state )
            {
                fl_set_button( obj, state );
                fs_button_callback_2d( obj, curve );
            }
        }
        else
        {
            old_state = G_2d.curve_2d[ curve - 1 ]->is_fs;

            if ( state != -1 && old_state != state )
                G_2d.curve_2d[ curve - 1 ]->is_fs = state;
        }

        writer( C_FSB_2D_REPLY, old_state );

        EDL.Fname = old_Fname;
        EDL.Lc = old_Lc;
    }

    return SET;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
