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


#include <dlfcn.h>
#include "fsc2.h"
#include <stdarg.h>


/*-----------------------------------------------------*
 * This function is called by modules to determine the
 * current mode without them being able to change it.
 *-----------------------------------------------------*/

int
get_mode( void )
{
    return Fsc2_Internals.mode;
}


/*-------------------------------------------------------------*
 * This function is called by modules to determine if this is
 * just a check run without graphical interface or a check run
 *-------------------------------------------------------------*/

int
get_check_state( void )
{
    return Fsc2_Internals.cmdline_flags &
           ( DO_CHECK | TEST_ONLY | NO_GUI_RUN );
}


/*-------------------------------------------------------------*
 * This function is called by modules to determine if this is
 * we're running in batch mode.
 *-------------------------------------------------------------*/

int
get_batch_state( void )
{
    return Fsc2_Internals.cmdline_flags & BATCH_MODE;
}


/*---------------------------------------------------------------*
 * Function returns true if the user has hit the "Stop" button.
 * It's the users responsibility to throw a USER_BREAK_EXCEPTION
 * in this case.
 *---------------------------------------------------------------*/

bool
check_user_request( void )
{
    if (    Fsc2_Internals.I_am == PARENT
         && ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        fl_check_only_forms( );
    return EDL.do_quit && EDL.react_to_do_quit;
}


/*------------------------------------------------------*
 * Function tests if the user has hit the "Stop" button
 * and throws an USER_BREAK_EXCEPTION in this case.
 *------------------------------------------------------*/

void
stop_on_user_request( void )
{
    if (    Fsc2_Internals.I_am == PARENT
         && ! ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN ) )
        fl_check_only_forms( );
    if ( EDL.do_quit && EDL.react_to_do_quit )
        THROW( USER_BREAK_EXCEPTION );
}


/*--------------------------------------------------------------*
 * This function might be called to check if there are any more
 * variables on the variable stack, representing superfluous
 * arguments to a function.
 *--------------------------------------------------------------*/

void
too_many_arguments( Var_T * v )
{
    if ( v == NULL || ( v = vars_pop( v ) ) == NULL )
        return;

    print( WARN, "Too many arguments, discarding superfluous argument%s.\n",
           v->next != NULL ? "s" : "" );

    while ( ( v = vars_pop( v ) ) != NULL )
        /* empty */ ;
}


/*------------------------------------------------------------*
 * This function just tells the user that a function can't be
 * called ithout an argument (unless we are in the EXPERIMENT
 * section) and then trows an exception.
 *------------------------------------------------------------*/

void
no_query_possible( void )
{
    print( FATAL, "Function can be used for queries in the EXPERIMENT "
           "section only.\n" );
    THROW( EXCEPTION );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

long
get_long( Var_T      * restrict v,
          const char * restrict snippet )
{
    vars_check( v, INT_VAR | FLOAT_VAR );

    if ( v->type == FLOAT_VAR && snippet != NULL )
        print( WARN, "Floating point number used as %s.\n", snippet );

    if (    v->type == FLOAT_VAR
         && ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN ) )
        print( SEVERE, "Integer argument overflow.\n" );

    return v->type == INT_VAR ? v->val.lval : lrnd( v->val.dval );
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

double
get_double( Var_T      * restrict v,
            const char * restrict snippet )
{
    vars_check( v, INT_VAR | FLOAT_VAR );

    if ( v->type == INT_VAR && snippet != NULL )
        print( WARN, "Integer number used as %s.\n", snippet );

    return v->type == INT_VAR ? v->val.lval : v->val.dval;
}


/*----------------------------------------------------------------------*
 * This function can be called to get the value of a variable that must
 * be an integer variable. If it isn't an error message is printed and
 * only when the program is interpreting the EXPERIMENT section and the
 * variable is a floating point variable no exception is thrown but its
 * value is converted to an int and this value returned.
 *----------------------------------------------------------------------*/

long
get_strict_long( Var_T      * restrict v,
                 const char * restrict snippet )
{
    vars_check( v, INT_VAR | FLOAT_VAR );

    if ( v->type == FLOAT_VAR )
    {
        if ( Fsc2_Internals.mode == EXPERIMENT )
        {
            if ( snippet != NULL )
                print( SEVERE, "Floating point number used as %s, "
                       "trying to continue!\n", snippet );

            if ( v->val.dval > LONG_MAX || v->val.dval < LONG_MIN )
                print( SEVERE, "Integer argument overflow.\n" );

            return lrnd( v->val.dval );
        }

        if ( snippet != NULL  )
            print( FATAL, "Floating point number can't be used as %s.\n",
                   snippet );
        THROW( EXCEPTION );
    }

    return v->val.lval;
}


/*---------------------------------------------------------------------------*
 * This function can be called when a variable that should contain a boolean
 * is expected, where boolean means either an integer (where 0 corresponds
 * to FALSE and a non-zero value means TRUE) or a string, either "ON" or
 * "OFF" (capitalization doesn't matter). If the value is a floating point
 * it is accepted (after printing an error message) during the EXPERIMENT
 * only, otherwise an exception is thrown. When it's a string and doesn't
 * match either "ON" or "OFF" and exception is thrown in every case.
 *---------------------------------------------------------------------------*/

bool
get_boolean( Var_T * v )
{
    const char *alt[ 2 ] = { "OFF", "ON" };
    int res;


    vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

    if ( v->type == FLOAT_VAR )
    {
        if ( Fsc2_Internals.mode != EXPERIMENT )
        {
            print( FATAL, "Floating point number found where boolean value "
                   "was expected.\n" );
            THROW( EXCEPTION );
        }

        print( SEVERE, "Floating point number found where boolean value "
               "was expected, trying to continue.\n" );
        return v->val.dval != 0.0;
    }
    else if ( v->type == STR_VAR )
    {
        if ( ( res = is_in( v->val.sptr, alt, 2 ) ) >= 0 )
            return res != 0 ? SET : UNSET;

        print( FATAL, "Invalid boolean argument (\"%s\").\n",  v->val.sptr );
        THROW( EXCEPTION );
    }

    return v->val.lval != 0;
}


/*---------------------------------------------------------------------------*
 * Function tests if the time (in seconds) it gets passed is a reasonable
 * integer multiple of 1 ns and tries to reduce rounding errors. If the time
 * is more than 10 ps off from a multiple of a nanosecond an error message
 * is output, using the piece of text passed to the function as the second
 * argument.
 *---------------------------------------------------------------------------*/

double
is_mult_ns( double       val,
            const char * text )
{
    double ip, fp;


    val *= 1.0e9;
    fp = modf( val , &ip );

    if ( fabs( fp ) > 1.e-2 && fabs( fp ) < 0.99 )
    {
        print( FATAL, "%s must be an integer multiple of 1 ns.\n", text );
        THROW( EXCEPTION );
    }

    if ( ip < 0.0 && fp < -0.5 )
        return ( ip - 1.0 ) * 1.0e-9;
    if ( ip >= 0.0 && fp > 0.5 )
        return ( ip + 1.0 ) * 1.0e-9;;

    return ip * 1.0e-9;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

char *
translate_escape_sequences( char * str )
{
    return handle_escape( str );
}


/*-------------------------------------------------------------------*
 * This function can be used by modules to get a very rough estimate
 * of the time since the start of the test run. This is useful for
 * example in cases where an automatic sweep is done and during the
 * test run an estimate for the swept value has to be returned that
 * is not completely bogus. But take care, the value returned by
 * the function might easily be off by an order of magnitude.
 * The method to estimate the time is as simple as possible: To the
 * real time used for interpretating the EDL program is added the
 * time spend in calls of the EDL function wait() (see f_wait() in
 * func_util.c) plus the value MODULE_CALL ESTIMATE (defined in the
 * header file) for each module function call.
 * During the experiment the function returns the actual time since
 * the start of the experiment.
 *-------------------------------------------------------------------*/

double
experiment_time( void )
{
    struct timeval t_new;
    static struct timeval t_old = { 0, 0 };
    double delta;


    gettimeofday( &t_new, NULL );
    delta = t_new.tv_sec  - t_old.tv_sec
            + 1.0e-6 * ( t_new.tv_usec - t_old.tv_usec );
    t_old  = t_new;

    return EDL.experiment_time += delta;
}


/*------------------------------------------------------------------*
 * Function for opening files with the maximum permissions of fsc2.
 * Close-on-exec flag gets set for the newly opened file.
 *------------------------------------------------------------------*/

FILE *
fsc2_fopen( const char * restrict path,
            const char * restrict mode )
{
    raise_permissions( );
    FILE * fp = fopen( path, mode );

    /* Probably rarely necessary, but make sure the close-on-exec flag is
       se for the file */

    if ( fp )
    {
        int fd_flags = fcntl( fileno( fp ), F_GETFD );

        if ( fd_flags < 0 )
            fd_flags = 0;
        fcntl( fileno( fp ), F_SETFD, fd_flags | FD_CLOEXEC );
    }

    lower_permissions( );
    return fp;
}


/*---------------------------------------------------------*
 * Function for formated reading from a file with the full
 * permissions of fsc2
 *---------------------------------------------------------*/

int
fsc2_fscanf( FILE       * restrict stream,
             const char * restrict format,
             ... )
{
    va_list ap;
    int num;


    va_start( ap, format );
    raise_permissions( );
    num = vfscanf( stream, format, ap );
    lower_permissions( );
    va_end( ap );
    return num;
}


/*--------------------------------------------------*
 * Function for unformated reading from a file with
 * the full permissions of fsc2
 *--------------------------------------------------*/

size_t
fsc2_fread( void   * restrict ptr,
            size_t            size,
            size_t            nmemb,
            FILE   * restrict stream )
{
    size_t num;


    raise_permissions( );
    num = fread( ptr, size, nmemb, stream );
    lower_permissions( );
    return num;
}


/*-------------------------------------------------------*
 * Function for formated writing to a file with the full
 * permissions of fsc2
 *-------------------------------------------------------*/

int
fsc2_fprintf( FILE       * restrict stream,
              const char * restrict format,
              ... )
{
    va_list ap;
    int num;


    va_start( ap, format );
    raise_permissions( );
    num = vfprintf( stream, format, ap );
    fflush( stream );
    lower_permissions( );
    va_end( ap );
    return num;
}


/*------------------------------------------------*
 * Function for unformated writing to a file with
 * the full permissions of fsc2
 *------------------------------------------------*/

size_t
fsc2_fwrite( const void * restrict ptr,
             size_t                size,
             size_t                nmemb,
             FILE       * restrict stream )
{
    size_t num;


    raise_permissions( );
    num = fwrite( ptr, size, nmemb, stream );
    fflush( stream );
    lower_permissions( );
    return num;
}


/*------------------------------------------------*
 * Function for reading a single char from a file
 * with the full permissions of fsc2
 *------------------------------------------------*/

int
fsc2_fgetc( FILE * stream )
{
    int num;


    raise_permissions( );
    num = fgetc( stream );
    lower_permissions( );
    return num;
}


/*------------------------------------------------*
 * Function for reading a single char from a file
 * with the full permissions of fsc2
 *------------------------------------------------*/

int
fsc2_getc( FILE * stream )
{
    int num;

    raise_permissions( );
    num = getc( stream );
    lower_permissions( );
    return num;
}


/*-------------------------------------------*
 * Function for reading a string from a file
 * with the full permissions of fsc2
 *-------------------------------------------*/

char *
fsc2_fgets( char * restrict s,
            int             size,
            FILE * restrict stream )
{
    char *p;


    raise_permissions( );
    p = fgets( s, size, stream );
    lower_permissions( );
    return p;
}


/*---------------------------------------------------*
 * Function for pushing back a character into a file
 * with the full permissions of fsc2
 *---------------------------------------------------*/

int
fsc2_ungetc( int    c,
             FILE * stream )
{
    int num;


    raise_permissions( );
    num = ungetc( c, stream );
    lower_permissions( );
    return num;
}


/*---------------------------------------------*
 * Function for positioning in a file with the
 * full permissions of fsc2
 *---------------------------------------------*/

int
fsc2_fseek( FILE * stream,
            long   offset,
            int    whence )
{
    int num;

    raise_permissions( );
    num = fseek( stream, offset, whence );
    lower_permissions( );
    return num;
}


/*-------------------------------------------------*
 * Function for determining the position in a file
 * with the full permissions of fsc2
 *-------------------------------------------------*/

long
fsc2_ftell( FILE * stream )
{
    long num;


    raise_permissions( );
    num = ftell( stream );
    lower_permissions( );
    return num;
}


/*----------------------------------------------*
 * Function for writing a single char to a file
 * with the full permissions of fsc2
 *----------------------------------------------*/

int
fsc2_fputc( int    c,
            FILE * stream )
{
    int num;


    raise_permissions( );
    num = fputc( c, stream );
    fflush( stream );
    lower_permissions( );
    return num;
}


/*-----------------------------------------*
 * Function for writing a string to a file
 * with the full permissions of fsc2
 *-----------------------------------------*/

int
fsc2_fputs( const char * restrict s,
            FILE       * restrict stream )
{
    int num;


    raise_permissions( );
    num = fputs( s, stream );
    fflush( stream );
    lower_permissions( );
    return num;
}


/*----------------------------------------------*
 * Function for writing a single char to a file
 * with the full permissions of fsc2
 *----------------------------------------------*/

int
fsc2_putc( int    c,
           FILE * stream )
{
    int num;


    raise_permissions( );
    num = putc( c, stream );
    fflush( stream );
    lower_permissions( );
    return num;
}


/*---------------------------------------------------------------*
 * Function for closing a file with the full permissions of fsc2
 *---------------------------------------------------------------*/

int
fsc2_fclose( FILE * stream )
{
    int num;


    raise_permissions( );
    num = fclose( stream );
    lower_permissions( );
    return num;
}


/*---------------------------------------------------------------*
 *---------------------------------------------------------------*/

const char *
fsc2_config_dir( void )
{
    return ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) ) ?
           confdir : libdir;
}


/*----------------------------------------------------*
 * Helper function for outputting floating point data in a scientific
 * notation that is easy to read for the user. Returns an allocated
 * string the caller has to deallocated using T_free(). The first
 * argument is the number to output, the second the buffer to print to
 * and the third a unit of measure to append at the end.
 *----------------------------------------------------*/

char *
pretty_print( double                val,
              char       * restrict buf,
              char const * restrict unit )
{
    double aval = fabs( val );

    if ( aval >= 0.999995 )
    {
        if ( aval < 0.999995e3 )
            snprintf( buf, PP_BUF_LEN, "%.6g %s", val, unit );
        else if ( aval < 0.999995e6 )
            snprintf( buf, PP_BUF_LEN, "%.6g k%s", val * 1.0e-3, unit );
        else if ( aval < 0.999995e9 )
            snprintf( buf, PP_BUF_LEN, "%.6g M%s", val * 1.0e-6, unit );
        else
            snprintf( buf, PP_BUF_LEN, "%.6g G%s", val * 1.0e-9, unit );
    }
    else if ( aval >= 0.999995e-3 )
        snprintf( buf, PP_BUF_LEN, "%.6g m%s", val * 1.0e3, unit );
    else if ( aval >= 0.999995e-6 )
        snprintf( buf, PP_BUF_LEN, "%.6g u%s", val * 1.0e6, unit );
    else
        snprintf( buf, PP_BUF_LEN, "%.3g n%s", val * 1.0e9, unit );

    return buf;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
