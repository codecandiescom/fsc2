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


/* Both variables are defined in 'func.c' */

extern bool No_File_Numbers;           /* defined in func.c */
extern bool Dont_Save;                 /* defined in func.c */

static bool STD_Is_Open = false;


static Var_T * f_openf_int( Var_T         * /* v */,
                            volatile bool   /* do_compress */ );

static Var_T * f_getf_int( Var_T         * /* v */,
                           volatile bool   /* do_compress */ );

static Var_T * f_clonef_int( Var_T * /* v */,
                             bool    /* do_compress */ );

static int get_save_file( Var_T ** /* v */ );

static Var_T * batch_mode_file_open( char * /* name */,
                                     bool   /* do_compress */ );

static long arr_save( const char * sep,
                      long         file_num,
                      Var_T      * v );

static void f_format_check( Var_T * v );

static void ff_format_check( Var_T * v );

static long do_printf( long    file_num,
                       Var_T * v );

static long print_browser( int          browser,
                           int          fid,
                           const char * comment );

static long print_include( int          fid,
                           char       * volatile cp,
                           const char * volatile comment,
                           const char * cur_file );

static long T_fprintf( long         fn,
                       const char * fmt,
                       ... );

static const char * get_name( Var_T * v );


/*----------------------------------------------------------------*
 * Returns if a file number passed to the function stands for an
 * open file.
 *----------------------------------------------------------------*/

Var_T *
f_is_file( Var_T * v )
{
    long fn = get_long( v, "file number" );

    if ( STD_Is_Open && ( fn == 1 || fn == 2 ) )
        return vars_push( INT_VAR, 1L );

    fn -= FILE_NUMBER_OFFSET;

    if ( fn < 0 || fn >= EDL.File_List_Len )
        return vars_push( INT_VAR, 0L );

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, 1L );

    File_List_T * fl = EDL.File_List + fn;
    return vars_push( INT_VAR, (    (   fl->gzip && fl->gp )
                                 || ( ! fl->gzip && fl->fp ) ) ? 1L : 0L );
}


/*----------------------------------------------------------------*
 * Returns for a file number passed to the function the name of
 * that file (without path)
 *----------------------------------------------------------------*/

Var_T *
f_file_name( Var_T * v )
{
    const char *name = get_name( v );
    char * n = strrchr( name, '/' );

    if ( n )
        return vars_push( STR_VAR, n + 1 );
    else
        return vars_push( STR_VAR, name );
}


/*----------------------------------------------------------------*
 * Returns for a file number passed to the function the name of
 * that file with the complete path
 *----------------------------------------------------------------*/

Var_T *
f_path_name( Var_T * v )
{
    return vars_push( STR_VAR, get_name( v ) );
}


/*--------------------------------------------------------------*
 * Utility function for finding the file name for a file number
 *--------------------------------------------------------------*/

static
const char *
get_name( Var_T * v )
{
    /* The function can be called without an argument when we're in
       automatical file open mode, i.e. 'No_File_Numbers' is set */

    if ( ! v )
    {
        if ( ! No_File_Numbers )
        {
            print( FATAL, "Missing argument.\n" );
            THROW( EXCEPTION );
        }

        if ( EDL.File_List_Len < 3 )
        {
            print( FATAL, "No file has been opened yet.\n" );
            THROW( EXCEPTION );
        }

        if ( Fsc2_Internals.mode == TEST )
            return "dummy for test run";

        return EDL.File_List[ EDL.File_List_Len - 1 ].name;
    }

    /* Otherwise we need a correct file number */

    long fn = get_long( v, "file number" );

    /* Check for standard output and error */

    if ( STD_Is_Open && ( fn == 1 || fn == 2 ) )
        return fn == 1 ? "stdout" : "stderr";

    fn -= FILE_NUMBER_OFFSET;

    if ( fn == 0 || fn == 1 )
        return fn == 0 ? "stdout" : "stderr";

    if ( fn < 0 || fn >= EDL.File_List_Len )
    {
        print( FATAL, "Invalid file number\n" );
        THROW( EXCEPTION );
    }

    /* During the test run we can only return a dummy name */

    if ( Fsc2_Internals.mode == TEST )
        return "dummy for test run";

    return EDL.File_List[ fn ].name;
}


/*---------------------------------------------------------------------------*
 * Function allows to get a file with a predefined name for saving data. If
 * the file already exists and the user does not want it to be overwritten
 * (or the file name is an empty string) the function works exactly as the
 * the f_getf() function, see below.
 * Arguments:
 * 1. File name
 * 2. Message string (optional, not allowed to start with a backslash '\'!)
 * 3. Default pattern for file name (optional)
 * 4. Default directory (optional)
 * 5. Default file name (optional)
 * 6. Default extension to add (in case it's not already there) (optional)
 *---------------------------------------------------------------------------*/

Var_T *
f_openf( Var_T * v )
{
    return f_openf_int( v, false );
}


Var_T *
f_opengzf( Var_T * v )
{
    return f_openf_int( v, true );
}


static
Var_T *
f_openf_int( Var_T         * v,
             volatile bool   do_compress )
{
    FILE * volatile fp = NULL;
    gzFile volatile gp = NULL;

    /* If there was a call of 'f_save()' etc. without a previous call to
       'f_getf()' or 'f_openf()' then 'f_save()' (or one of its brethrens)
       already called 'f_getf()' by itself and from now on does not expect
       file identifiers anymore - in this case the variable 'No_File_Numbers'
       is set. So, if we get a call to 'f_getf()' with 'No_File_Numbers' being
       set we must tell the user that he can't have it both ways, i.e. (s)he
       either has to call 'f_getf()' or 'fopenf()' before any call to
       'f_save()' etc. or never. */

    if ( No_File_Numbers )
    {
        print( FATAL, "Function can't be called if one of the functions for "
               "writing data to a file already has been invoked without "
               "opening a file explicitely before.\n" );
        THROW( EXCEPTION );
    }

    if ( ! v )
    {
        print( FATAL, "Missing argument(s).\n" );
        THROW( EXCEPTION );
    }

    /* If there's only a single integer argument and its value is 1 or 2
       we're asked to "open" stdout or stderr */

    if (    v->type == INT_VAR
         && ! v->next
         && (    v->val.lval == STDOUT_FILENO
              || v->val.lval == STDERR_FILENO ) )
    {
        STD_Is_Open = true;
        return vars_push( INT_VAR, v->val.lval );
    }

    /* During test run just do a plausibilty check of the variables and
       return a dummy value */

    if ( Fsc2_Internals.mode == TEST )
    {
        Var_T * cur = v;
        for ( int i = 0; i < 6 && cur; i++, cur = cur->next )
            vars_check( cur, STR_VAR );

        return vars_push( INT_VAR, EDL.File_List_Len++ + FILE_NUMBER_OFFSET );
    }

    /* Check the arguments and supply default values if necessary */

        Var_T * cur = v;
    for ( int i = 0; i < 6 && cur; i++, cur = cur->next )
        vars_check( cur, STR_VAR );

    char * volatile fn = v->val.sptr;

    if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
    {
        do_compress = false;
        goto got_file;
    }

    if (    Fsc2_Internals.cmdline_flags & BATCH_MODE
         || Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
        return batch_mode_file_open( ! *fn ? NULL : fn, do_compress );

    if ( ! *fn )
        return f_getf_int( v->next, do_compress );

    /* Check if the file is already open */

    for ( int i = 0; i < EDL.File_List_Len; i++ )
    {
        if ( ! strcmp( EDL.File_List[ i ].name, fn ) )
        {
            char * m = get_string( "The specified file is already open:\n%s\n"
                                   "\nOpen another file instead?", fn );

            if ( 1 == show_choices( m, 2, "Yes", "No", NULL, 2, true ) )
            {
                T_free( m );
                return f_getf( v->next );
            }
            
            T_free( m );
            return vars_push( INT_VAR, i + FILE_NUMBER_OFFSET );
        }
    }

    /* Now ask for confirmation if the file already exists and try to open
       it for writing */

    struct stat stat_buf;
    if  ( stat( fn, &stat_buf ) == 0 )
    {
        char * m = get_string( "The specified file already exists:\n%s\n"
                               "\nDo you really want to overwrite it?", fn );

        if ( 2 == show_choices( m, 2, "Yes", "No", NULL, 2, true ) )
        {
            T_free( m );
            return f_getf( v->next );
        }

        T_free( m );
    }

    if (    ( ! do_compress && ! ( fp = fopen( fn, "w"    ) ) )
         || (   do_compress && ! ( gp = gzopen( fn, "wb9" ) ) ) )
    {
        switch ( errno )
        {
            case EMFILE :
                show_message( "You have too many open files!\n"
                              "Please close at least one and retry." );
                break;

            case ENFILE :
                show_message( "The system limit for open files is exceeded!\n"
                              " Please try to close some files and retry." );
                break;

            case ENOSPC :
                show_message( "No space left on device for more file!\n"
                              "  Please delete some files and retry." );
                break;

            case 0 :
                if ( do_compress )
                {
                    show_message( "Not enough memory for compressing the "
                                  "file. Opening it failed." );
                    print( FATAL, "Opening file for compressed writing failed "
                           "due to insufficient memory.\n" );
                    THROW( EXCEPTION );
                }
                /* fall through */

            default :
                show_message( "Can't open selected file for writing!\n"
                              "    Please select a different file." );
        }

        return f_getf_int( v->next, do_compress );
    }

    File_List_T * volatile old_File_List = NULL;

 got_file:

    /* The reallocation for the EDL.File_List may fail and we may need to
       close all files and get rid of memory for the file names when
       stopping the experiment, thus we save a copy of the current
       EDL.File_List before we try to reallocate (if allocation already
       fails while making the copy this isn't a problem!) */

    TRY
    {
        if ( EDL.File_List )
        {
            old_File_List = T_malloc(   EDL.File_List_Len
                                      * sizeof *old_File_List );
            memcpy( old_File_List, EDL.File_List,
                    EDL.File_List_Len * sizeof *old_File_List );
        }
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        if ( ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            if ( ! do_compress && fp )
                fclose( fp );
            else if ( do_compress && gp )
                gzclose( gp );
        }
        THROW( EXCEPTION );
    }

    TRY
    {
        EDL.File_List = T_realloc( EDL.File_List,
                                     ( EDL.File_List_Len + 1 )
                                   * sizeof *EDL.File_List );
        if ( old_File_List )
            T_free( old_File_List );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        EDL.File_List = old_File_List;
        if ( ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            if ( ! do_compress && fp )
                fclose( fp );
            else if ( do_compress && gp )
                gzclose( gp );
        }
        THROW( EXCEPTION );
    }

    if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        EDL.File_List[ EDL.File_List_Len ].fp = stdout;
    else
    {
        EDL.File_List[ EDL.File_List_Len ].fp = fp;
        EDL.File_List[ EDL.File_List_Len ].gp = gp;
        EDL.File_List[ EDL.File_List_Len ].gzip = do_compress;
    }

    EDL.File_List[ EDL.File_List_Len ].name = NULL;
    EDL.File_List[ EDL.File_List_Len ].name = T_strdup( fn );

    /* Switch off buffering for the newly opened file so we're sure
       everything gets written to disk without delay */

    if ( ! do_compress )
        setbuf( EDL.File_List[ EDL.File_List_Len ].fp, NULL );

    STD_Is_Open = true;
    return vars_push( INT_VAR, EDL.File_List_Len++ + FILE_NUMBER_OFFSET );
}


/*---------------------------------------------------------------------------*
 * Function allows the user to select a file using the file selector. If the
 * file already exists a confirmation by the user is required. Then the file
 * is opened - if this fails tell the user about the reasons and show the
 * file selector again. The new FILE pointer returned is stored in the array
 * of FILE pointers of open file descriptors. The return value is an INT_VAR,
 * which is the index in the FILE  *pointer array (plus an offset) or -1 if
 * no file was selected.
 * Optional input variables (each will replaced by a default string if the
 * argument is either NULL, the empty string or missing) are:
 * 1. Message string (not allowed to start with a backslash '\'!)
 * 2. Default pattern for file name
 * 3. Default directory
 * 4. Default file name
 * 5. Default extension to add (in case it's not already there)
 *---------------------------------------------------------------------------*/

Var_T *
f_getf( Var_T * v )
{
    return f_getf_int( v, false );
}


Var_T *
f_getgzf( Var_T * v )
{
    return f_getf_int( v, true );
}


static
Var_T *
f_getf_int( Var_T        * v,
           volatile bool   do_compress )
{
    FILE * volatile fp = NULL;
    gzFile volatile gp = NULL;


    /* If there was a call of 'f_save()' without a previous call to 'f_getf()'
       then 'f_save()' already called 'f_getf()' by itself and now does not
       expects file identifiers anymore - in this case 'No_File_Numbers' is
       set. So, if we get a call to 'f_getf()' while 'No_File_Numbers' is set
       we must tell the user that he can't have it both ways, i.e. (s)he
       either has to call 'f_getf()' before any call to 'f_save()' or never. */

    if ( No_File_Numbers )
    {
        print( FATAL, "Function can't be called if one of the save()-type "
               "functions already has been invoked.\n" );
        THROW( EXCEPTION );
    }

    /* During test run just do a plausibilty check of the variables and
       return a dummy value */

    if ( Fsc2_Internals.mode == TEST )
    {
        Var_T * cur = v;
        for ( int i = 0; i < 5 && cur; i++, cur = cur->next )
            vars_check( cur, STR_VAR );
        return vars_push( INT_VAR, EDL.File_List_Len++ + FILE_NUMBER_OFFSET );
    }

    /* Check the arguments and supply default values if necessary */

    char *s[ ] = { NULL, NULL, NULL, NULL, NULL };
    Var_T * cur = v;
    for ( int i = 0; i < 5 && cur;  i++, cur = cur->next )
    {
        vars_check( cur, STR_VAR );
        s[ i ] = cur->val.sptr;
    }

    if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
    {
        do_compress = false;
        goto got_file;
    }

    if (    Fsc2_Internals.cmdline_flags & BATCH_MODE
         || Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
        return batch_mode_file_open( NULL, do_compress );

    /* First string is the message */

    if ( ! s[ 0 ] || ! s[ 0 ][ 0 ] || s[ 0 ][ 0 ] == '\\' )
        s[ 0 ] = T_strdup( "Please select a file name:" );
    else
        s[ 0 ] = T_strdup( s[ 0 ] );

    /* Second string is the file name pattern */

    if ( ! s[ 1 ] || ! s[ 1 ][ 0 ] )
        s[ 1 ] = T_strdup( do_compress ? "*.dat.gz" : "*.dat" );
    else
        s[ 1 ] = T_strdup( s[ 1 ] );

    /* Third string is the default directory */

    if ( s[ 2 ] )
    {
        if ( ! s[ 2 ][ 0 ] )
            s[ 2 ] = NULL;
        else
            s[ 2 ] = T_strdup( s[ 2 ] );
    }

    if ( ! s[ 3 ] )
        s[ 3 ] = T_strdup( "" );
    else
        s[ 3 ] = T_strdup( s[ 3 ] );

    if ( ! s[ 4 ] || ! s[ 4 ][ 0 ] )
        s[ 4 ] = NULL;
    else
        s[ 4 ] = T_strdup( s[ 4 ] );

    char * volatile r = NULL;

 getfile_retry:

    /* Try to get a filename - on 'Cancel' request confirmation (unless a
       file name was passed to the routine and this is not a repeat call) */

    if ( ! r )
        r = T_strdup( show_fselector( s[ 0 ], s[ 2 ], s[ 1 ], s[ 3 ] ) );

    if (    ( ! r || ! *r )
         && show_choices( "Do you really want to cancel saving data?\n"
                          "        Those data will be lost!",
                          2, "Yes", "No", NULL, 2, true ) != 1 )
    {
        r = T_free( r );
        goto getfile_retry;
    }

    if ( ! r || ! *r )                    /* on 'Cancel' with confirmation */
    {
        T_free( r );
        for ( int i = 0; i < 5; i++ )
            T_free( s[ i ] );
        Dont_Save = true;
        return vars_push( INT_VAR, FILE_NUMBER_NOT_OPEN );
    }

    /* If given append default extension to the file name (but only if the
       user didn't enter one when telling us about the file name) */

    if (    s[ 4 ]
         && (    ! strrchr( r, '.' )
              || strcmp( strrchr( r, '.' ) + 1, s[ 4 ] ) ) )
    {
        char * new_r = get_string( "%s.%s", r, s[ 4 ] );
        T_free( r );
        r = new_r;
    }

    /* Check if the file is already open */

    for ( int i = 0; i < EDL.File_List_Len; i++ )
        if ( ! strcmp( EDL.File_List[ i ].name, r ) )
        {
            char * m = get_string( "The specified file is already open:\n%s\n"
                                   "\nOpen another file instead?", r );

            if ( 2 == show_choices( m, 2, "Yes", "No", NULL, 2, true ) )
            {
                T_free( m );
                return vars_push( INT_VAR, i + FILE_NUMBER_OFFSET );
            }
            
            T_free( m );
            goto getfile_retry;
        }

    for ( int i = 0; i < EDL.File_List_Len; i++ )
        if ( ! strcmp( EDL.File_List[ i ].name, r ) )
        {
            print( SEVERE, "File '%s' is already open.\n", r );
            return vars_push( INT_VAR, i + FILE_NUMBER_OFFSET );
        }

    /* Now ask for confirmation if the file already exists and try to open
       it for writing */

    struct stat stat_buf;
    if  ( 0 == stat( r, &stat_buf ) )
    {
        char * m = get_string( "The selected file does already exist:\n%s\n"
                               "\nDo you really want to overwrite it?", r );
        if ( 1 != show_choices( m, 2, "Yes", "No", NULL, 2, true ) )
        {
            T_free( m );
            r = T_free( r );
            goto getfile_retry;
        }
        T_free( m );
    }

    if (    ( ! do_compress && ! ( fp = fopen( r, "w+"   ) ) )
         || (   do_compress && ! ( gp = gzopen( r, "wb9" ) ) ) )
    {
        switch( errno )
        {
            case EMFILE :
                show_message( "You have too many open files!\n"
                              "Please close at least one and retry." );
                break;

            case ENFILE :
                show_message( "System limit for open files exceeded!\n"
                              "Please try to close some files and retry." );
                break;

            case ENOSPC :
                show_message( "No space left on device for more file!\n"
                              "Please delete some files and retry." );
                break;

            case 0 :
                if ( do_compress )
                {
                    r = T_free( r );
                    show_message( "Not enough memory for compressing the "
                                  "file. Opening it failed." );
                    print( FATAL, "Opening file for compressed writing failed "
                           "due to insufficient memory.\n" );
                    THROW( EXCEPTION );
                }
                /* fall through */

            default :
                show_message( "Can't open selected file for writing!\n"
                              "Please select a different file." );
        }

        r = T_free( r );
        goto getfile_retry;
    }

    File_List_T * volatile old_File_List = NULL;

 got_file:

    for ( int i = 0; i < 5; i++ )
        T_free( s[ i ] );

    /* The reallocation for the EDL.File_List may fail but we still need to
       close all files and get rid of memory for the file names, thus we save
       the current EDL.File_List before we try to reallocate */

    if ( EDL.File_List )
    {
        old_File_List = T_malloc( EDL.File_List_Len * sizeof *old_File_List );
        memcpy( old_File_List, EDL.File_List,
                EDL.File_List_Len * sizeof *old_File_List );
    }

    TRY
    {
        EDL.File_List = T_realloc( EDL.File_List,
                                     ( EDL.File_List_Len + 1 )
                                   * sizeof *EDL.File_List );
        if ( old_File_List )
            T_free( old_File_List );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        EDL.File_List = old_File_List;

        if ( ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            if ( ! do_compress && fp )
                fclose( fp );
            else if ( do_compress && gp )
                gzclose( gp );
        }

        THROW( EXCEPTION );
    }

    if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
        EDL.File_List[ EDL.File_List_Len ].fp = stdout;
    else
    {
        EDL.File_List[ EDL.File_List_Len ].fp = fp;
        EDL.File_List[ EDL.File_List_Len ].gp = gp;
        EDL.File_List[ EDL.File_List_Len ].gzip = do_compress;
    }

    EDL.File_List[ EDL.File_List_Len ].name = r;

    /* Switch off buffering for normal files so we're sure everything gets
       written to disk immediately */

    if ( ! do_compress )
        setbuf( EDL.File_List[ EDL.File_List_Len ].fp, NULL );

    STD_Is_Open = true;
    return vars_push( INT_VAR, EDL.File_List_Len++ + FILE_NUMBER_OFFSET );
}


/*-------------------------------------------------------------------*
 * Function called for the 'clone_file()' EDL function to open a new
 * file with a name made up from the one of an already opened file
 * after applying some changes according to the input arguments.
 *-------------------------------------------------------------------*/

Var_T *
f_clonef( Var_T * v )
{
    return f_clonef_int( v, false );
}


Var_T *
f_clonegzf( Var_T * v )
{
    return f_clonef_int( v, true );
}


static
Var_T *
f_clonef_int( Var_T * v,
              bool    do_compress )
{
    /* Neither stdout nor stderr an be "cloned" */

    if (    v->type == INT_VAR
         && ! No_File_Numbers
         && (    v->val.lval == FILE_NUMBER_STDOUT
              || v->val.lval == FILE_NUMBER_STDERR ) )
    {
        print( WARN, "std%s can't be cloned.\n",
               v->val.lval == FILE_NUMBER_STDOUT ? "out" : "err" );
        return vars_push( INT_VAR, FILE_NUMBER_STDERR );
    }

    /* If the file handle passed to the function is FILE_NUMBER_NOT_OPEN
       opening a file for this file handle did not happen, so we also don't
       open the new file */

    if ( v->type == INT_VAR && v->val.lval == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, FILE_NUMBER_NOT_OPEN );

    /* Check all the parameter */

    if (    v->type != INT_VAR
         || v->val.lval < FILE_NUMBER_OFFSET
         || v->val.lval >= EDL.File_List_Len + FILE_NUMBER_OFFSET )
    {
         print( FATAL, "First argument isn't a vaild file handle.\n" );
         THROW( EXCEPTION );
    }

    long file_num = v->val.lval - FILE_NUMBER_OFFSET;

    if ( v->next->type != STR_VAR )
    {
        print( FATAL, "Invalid second argument.\n" );
        THROW( EXCEPTION );
    }

    if (    v->next->next->type != STR_VAR
         || ! *v->next->next->val.sptr )
    {
        print( FATAL, "Invalid third argument.\n" );
        THROW( EXCEPTION );
    }

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, EDL.File_List_Len++ + FILE_NUMBER_OFFSET );

    char * fn = T_malloc(   strlen( EDL.File_List[ file_num ].name )
                            + strlen( v->next->next->val.sptr ) + 2 );
    strcpy( fn, EDL.File_List[ file_num ].name );

    char * n = fn + strlen( fn ) - strlen( v->next->val.sptr );
    if ( n > fn + 1 && *( n - 1 ) == '.' && ! strcmp( n, v->next->val.sptr ) )
        strcpy( n, v->next->next->val.sptr );
    else
    {
        strcat( n, "." );
        strcat( n, v->next->next->val.sptr );
    }

    Var_T * arg[ 6 ];

    arg[ 0 ] = vars_push( STR_VAR, fn );
    T_free( fn );

    arg[ 1 ] = vars_push( STR_VAR, "" );

    n = get_string( "*.%s", v->next->next->val.sptr );
    arg[ 2 ] = vars_push( STR_VAR, n );
    T_free( n );

    arg[ 3 ] = vars_push( STR_VAR, "" );
    arg[ 4 ] = vars_push( STR_VAR, "" );
    arg[ 5 ] = vars_push( STR_VAR, v->next->next->val.sptr );

    Var_T * new_v = f_openf_int( arg[ 0 ], do_compress );

    for ( int i = 5; i >= 0; i-- )
        vars_pop( arg[ i ] );

    return new_v;
}


/*-------------------------------------------------------------------*
 * Function called for the 'reset_file()' EDL function to truncate
 * a file.
 *-------------------------------------------------------------------*/

Var_T *
f_resetf( Var_T * v )
{
    /* Neither stdout nor stderr an be "cloned" */

    if (    v
         && v->type == INT_VAR
         && ! No_File_Numbers
         && (    v->val.lval == FILE_NUMBER_STDOUT
              || v->val.lval == FILE_NUMBER_STDERR ) )
    {
        print( WARN, "std%s can't be reset.\n",
               v->val.lval == FILE_NUMBER_STDOUT ? "out" : "err" );
        return vars_push( INT_VAR, FILE_NUMBER_STDERR );
    }

    /* Check that a possibly available parameter is ok. Also consider the
       case that the user didn't open the file that's now being reset. */

    if ( v )
    {
        if ( v->type == INT_VAR && v->val.lval == FILE_NUMBER_NOT_OPEN )
            return vars_push( INT_VAR, FILE_NUMBER_NOT_OPEN );

        if (    v->type != INT_VAR
             || v->val.lval < FILE_NUMBER_OFFSET
             || v->val.lval >= EDL.File_List_Len + FILE_NUMBER_OFFSET )
        {
            print( FATAL, "Argument isn't a valid file handle.\n" );
            THROW( EXCEPTION );
        }
    }

    long file_num;

    if ( v )
        file_num = v->val.lval - FILE_NUMBER_OFFSET;
    else if ( ! No_File_Numbers && EDL.File_List_Len > 2 )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }
    else if ( EDL.File_List_Len == 2 )
        return vars_push( INT_VAR, 0L );
    else
        file_num = 2;

    /* Nothing further to be done in test mode */

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, file_num + FILE_NUMBER_OFFSET );

    /* Truncate the file - do it both for the standard C API (which allows
       just setting the position) and the UNIX API */

    File_List_T * fl = EDL.File_List + file_num;

    if ( ! fl->gzip )
    {
        fflush( fl->fp );
        if (    fseek( fl->fp, 0, SEEK_SET ) != 0
             || lseek( fileno( fl->fp ), 0, SEEK_SET ) != 0 )
        {
            print( FATAL, "Failed to reset file.\n" );
            THROW( EXCEPTION );
        }

        while ( ftruncate( fileno( fl->fp ), 0 ) != 0 )
        {
            if ( errno == EINTR )
                continue;
            print( FATAL, "Failed to reset file.\n" );
            THROW( EXCEPTION );
        }
    }
    else
    {
        gzclose( fl->gp );
        if ( ! ( fl->gp = gzopen( fl->name, "wb9" ) ) )
        {
            print( FATAL, "Failed to reset file.\n" );
            THROW( EXCEPTION );
        }
    }

    return vars_push( INT_VAR, file_num + FILE_NUMBER_OFFSET );
}


/*--------------------------------------------------------*
 * Function for opening a file when running in batch mode
 *--------------------------------------------------------*/

static
Var_T *
batch_mode_file_open( char * name,
                      bool   do_compress )
{
    FILE * volatile fp = NULL;
    gzFile volatile gp = NULL;


    /* If no prefered file name is given (e.g. if we came here from f_getf())
       we make one up from the name of the currently executed EDL script
       (but only the name, not the path). We do so by appending an extension
       of ".batch_output.%lu", where %lu stands for an integer number that
       gets incremented until we have a file that does not yet exist.
       Otherwise, when a prefered name was given we try to use it, but if it
       already exists we also append an additional extension the same way. */

    unsigned long cn = 0;
    char * volatile new_name = NULL;

    if ( ! name )
    {
        struct stat stat_buf;
        do
        {
            if ( cn % 10 == 0 )
            {
                if ( new_name )
                    new_name = T_free( new_name );
                new_name = get_string( "%s.batch_output.%lu%s",
                                       strrchr( EDL.files->name, '/' ) + 1,
                                       cn++, do_compress ? ".gz" : "" );
            }
            else
                new_name[ strlen( new_name ) - 1 ] += 1;
        } while ( 0 == stat( new_name, &stat_buf ) );
    }
    else
    {
        struct stat stat_buf;
        if ( 0 == stat( name, &stat_buf ) )
        {
            do
            {
                if ( cn % 10 == 0 )
                {
                    if ( new_name )
                        new_name = T_free( new_name );
                    new_name = get_string( "%s.batch_output.%lu%s", name, cn++,
                                           do_compress ? ".gz" : "" );
                }
                else
                    new_name[ strlen( new_name ) - 1 ] += 1;
            } while ( 0 == stat( new_name, &stat_buf ) );
        }
        else
            new_name = T_strdup( name );
    }

    /* Now try to open the new file, if this fails we must give up */

    if ( strcmp( name, new_name ) )
        fprintf( stderr, "Opening file '%s' instead of requested file '%s'!\n",
                 new_name, name );

    if (    ( ! do_compress && ! ( fp = fopen( new_name, "w+"   ) ) )
         || (   do_compress && ! ( gp = gzopen( new_name, "wb9" ) ) ) )
    {
        switch( errno )
        {
            case EMFILE :
                fprintf( stderr, "Sorry, you have too many open files.\n" );
                break;

            case ENFILE :
                fprintf( stderr, "Sorry, system limit for open files "
                         "exceeded.\n" );
                break;

            case ENOSPC :
                fprintf( stderr, "Sorry, no space left on device for more "
                         "file.\n" );
                break;

            case 0 :
                if ( do_compress )
                {
                    print( FATAL, "Opening file for compressed writing failed "
                           "due to insufficient memory.\n" );
                    break;
                }
                /* fall through */

            default :
                fprintf( stderr, "Sorry, can't open file for writing.\n" );
        }

        THROW( EXCEPTION );
    }

    /* The reallocation for the EDL.File_List may fail but we still need to
       close all files and get rid of memory for the file names, thus we save
       the current EDL.File_List before we try to reallocate */

    File_List_T * volatile old_File_List = NULL;

    if ( EDL.File_List )
    {
        old_File_List = T_malloc( EDL.File_List_Len * sizeof *old_File_List );
        memcpy( old_File_List, EDL.File_List,
                EDL.File_List_Len * sizeof *old_File_List );
    }

    TRY
    {
        EDL.File_List = T_realloc( EDL.File_List,
                                     ( EDL.File_List_Len + 1 )
                                   * sizeof *EDL.File_List );
        if ( old_File_List )
            T_free( old_File_List );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        if ( ! do_compress && fp )
            fclose( fp );
        else if ( do_compress && gp )
            gzclose( gp );

        T_free( new_name );
        EDL.File_List = old_File_List;
        THROW( EXCEPTION );
    }

    EDL.File_List[ EDL.File_List_Len ].fp = fp;
    EDL.File_List[ EDL.File_List_Len ].gp = gp;
    EDL.File_List[ EDL.File_List_Len ].gzip = do_compress;
    EDL.File_List[ EDL.File_List_Len ].name = new_name;

    /* Switch buffering off so we're sure everything gets written to disk
       immediately */

    if ( ! do_compress )
        setbuf( EDL.File_List[ EDL.File_List_Len ].fp, NULL );

    return vars_push( INT_VAR, EDL.File_List_Len++ + FILE_NUMBER_OFFSET );
}


/*-------------------------------------------------------------------*
 * Closes and deletes an open file
 *-------------------------------------------------------------------*/

Var_T *
f_delf( Var_T * v )
{
    /* Neither stdout nor stderr an be deleted */

    if (    v
         && v->type == INT_VAR
         && ! No_File_Numbers
         && (    v->val.lval == FILE_NUMBER_STDOUT
              || v->val.lval == FILE_NUMBER_STDERR ) )
    {
        print( WARN, "std%s can't be deleted.\n",
               v->val.lval == FILE_NUMBER_STDOUT ? "out" : "err" );
        return vars_push( INT_VAR, 0L );
    }

    /* Check that a possibly available parameter is ok. Also consider the
       case that the user didn't open the file that's now being deleted. */

    if ( v )
    {
        if ( v->type == INT_VAR && v->val.lval == FILE_NUMBER_NOT_OPEN )
            return vars_push( INT_VAR, 1L );

        if (    v->type != INT_VAR
             || v->val.lval < FILE_NUMBER_OFFSET
             || v->val.lval >= EDL.File_List_Len + FILE_NUMBER_OFFSET )
        {
            print( FATAL, "Argument isn't a valid file handle.\n" );
            THROW( EXCEPTION );
        }
    }

    long file_num;

    if ( v )
        file_num = v->val.lval - FILE_NUMBER_OFFSET;
    else if ( ! No_File_Numbers && EDL.File_List_Len > 2 )
    {
        print( FATAL, "Missing argument.\n" );
        THROW( EXCEPTION );
    }
    else if ( EDL.File_List_Len == 2 )
        return vars_push( INT_VAR, 0L );
    else
        file_num = 2;

    /* Nothing further to be done in test mode */

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, 1L );

    File_List_T * fl = EDL.File_List + file_num;

    if ( ! fl->gzip && fl->fp )
    {
        fclose( fl->fp );
        fl->fp = NULL;
    }
    else if ( fl->gzip && fl->gp )
    {
        gzclose( fl->gp );
        fl->gp = NULL;
    }

    unlink( fl->name );

    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------------*
 * This function is called by the functions for saving. If they didn't
 * get a file identifier it is assumed the user wants just one file
 * that is opened at the first call of a function of the 'save_xxx()'
 * family of functions.
 *---------------------------------------------------------------------*/

static
int
get_save_file( Var_T ** v )
{
    /* If the first argument is an integer variable and has the value 1
       or 2 return the index for stdout or stderr - but only if they have
       been "opened" either via an explicit call of f_openf() with 1 or 2
       as the single argument. */

    long file_num;

    if (    *v
         && ( *v )->type == INT_VAR
         && ( STD_Is_Open || EDL.File_List_Len > 2 )
         && (    ( *v )->val.lval == STDOUT_FILENO
              || ( *v )->val.lval == STDERR_FILENO ) )
    {
        if ( ! STD_Is_Open )
        {
            print( FATAL, "The std%s handle must have been opened by an "
                   "explicit call of open_file().\n",
                   ( *v )->val.lval == STDOUT_FILENO ? "out" : "err" );
            THROW( EXCEPTION );
        }

        file_num = ( *v )->val.lval == STDOUT_FILENO ?
                   FILE_NUMBER_STDOUT : FILE_NUMBER_STDERR;
        *v = vars_pop( *v );
        return ( int ) file_num;
    }

    /* If no file has been selected yet get a file and then use it exclusively
       (i.e. also expect that no file identifier is given in later calls),
       otherwise the first variable has to be the file identifier. We compare
       the length of the list to two because the first and second entry always
       already exist for stdout and stderr. */

    if ( EDL.File_List_Len == 2 )
    {
        if ( Dont_Save )
            return FILE_NUMBER_NOT_OPEN;

        No_File_Numbers = false;

        Var_T *get_file_ptr = func_get( "get_file", NULL );
        Var_T * file = func_call( get_file_ptr );      /* get the file name */

        No_File_Numbers = true;

        if ( file->val.lval == FILE_NUMBER_NOT_OPEN )
        {
            vars_pop( file );
            Dont_Save = true;
            return FILE_NUMBER_NOT_OPEN;
        }

        vars_pop( file );
        file_num = FILE_NUMBER_OFFSET + 2;
    }
    else if ( ! No_File_Numbers )                    /* file number is given */
    {
        if ( *v )
        {
            /* Check that the first variable is an integer, i.e. can be a
               file identifier */

            if ( ( *v )->type != INT_VAR )
            {
                print( FATAL, "First argument isn't a file handle.\n" );
                THROW( EXCEPTION );
            }
            file_num = ( int ) ( *v )->val.lval;
        }
        else
        {
            print( WARN, "Missing arguments.\n" );
            return FILE_NUMBER_NOT_OPEN;
        }
        *v = vars_pop( *v );
    }
    else
        file_num = FILE_NUMBER_OFFSET + 2;

    /* Check that the file identifier is reasonable */

    if ( file_num == FILE_NUMBER_NOT_OPEN )
    {
        if ( ! Dont_Save )
            print( WARN, "File has never been opened, skipping command.\n" );
        return FILE_NUMBER_NOT_OPEN;
    }

    if (    file_num < FILE_NUMBER_OFFSET
         || file_num >= EDL.File_List_Len + FILE_NUMBER_OFFSET )
    {
        print( FATAL, "Invalid file handle.\n" );
        THROW( EXCEPTION );
    }

    return file_num;
}


/*--------------------------------*
 * Closes all opened output files
 *--------------------------------*/

void
close_all_files( void )
{
    /* Return immediately if only the file entries for stdout and stderr
       are in the list */

    if ( EDL.File_List_Len == 2 )
        return;

    for ( int i = 2; i < EDL.File_List_Len; i++ )
    {
        if ( ! EDL.File_List[ i ].gzip && EDL.File_List[ i ].fp )
            fclose( EDL.File_List[ i ].fp );
        else if ( EDL.File_List[ i ].gzip && EDL.File_List[ i ].gp )
            gzclose( EDL.File_List[ i ].gp );

        if ( EDL.File_List[ i ].name )
            T_free( EDL.File_List[ i ].name );
    }

    EDL.File_List = T_realloc( EDL.File_List, 2 * sizeof *EDL.File_List );
    EDL.File_List_Len = 2;
    STD_Is_Open = false;
}


/*----------------------------------------------------------------------*
 * Saves data to a file. If 'get_file()' hasn't been called yet it will
 * be called now - in this case the file opened this way is the only
 * file to be used and no file identifier is allowed as first argument
 * to 'save()'. This version of save writes the data in an unformatted
 * way, i.e. each on its own line with the only exception of arrays of
 * more than one dimension where an empty line is put between the
 * slices. It returns the number of characters written.
 *----------------------------------------------------------------------*/

Var_T *
f_save( Var_T * v )
{
    /* Check if there's a separator character for arrays as the very
       first argument */

    char *sep = NULL;
    if ( v->type == STR_VAR )
    {
        sep = T_strdup( v->val.sptr );
        v = vars_pop( v );
    }

    /* Determine the file identifier */

    long file_num = get_save_file( &v );
    if ( file_num == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, 0L );

    if ( ! v )
    {
        print( WARN, "Missing arguments.\n" );
        return vars_push( INT_VAR, 0L );
    }

    long count = 0;

    do
    {
        switch( v->type )
        {
            case INT_VAR :
                if ( sep )
                    print( WARN, "Separator has no effect with integers "
                           "data\n" );
                count += T_fprintf( file_num, "%ld\n", v->val.lval );
                break;

            case FLOAT_VAR :
                if ( sep )
                    print( WARN, "Separator has no effect with float data\n" );
                count += T_fprintf( file_num, "%#.9g\n", v->val.dval );
                break;

            case STR_VAR :
                if ( sep )
                    print( WARN, "Separator has no effect with string data\n" );
                count += T_fprintf( file_num, "%s\n",
                                    handle_escape( v->val.sptr ) );
                break;


            case INT_ARR : case FLOAT_ARR : case INT_REF : case FLOAT_REF :
                count += arr_save( sep, file_num, v );
                break;

            default :
                fsc2_impossible( );
        }
    } while ( ( v = vars_pop( v ) ) );

    if ( sep )
        T_free( sep );

    return vars_push( INT_VAR, count );
}


/*-------------------------------------------------------------------------*
 * Function called when a one- or more-dimensional variable is passed to
 * the 'save()' EDL function. It writes out the array one element per line
 * unless a separator is specified, then a values are separated by this on
 * a line and the newline character only gets appended to the end.
 *-------------------------------------------------------------------------*/

static
long
arr_save( const char * sep,
          long         file_num,
          Var_T      * v )
{
    long count = 0;
    ssize_t i;

    switch ( v->type )
    {
        case INT_ARR :
            if ( sep )
            {
                for ( i = 0; i < v->len - 1; i++ )
                    count += T_fprintf( file_num, "%ld%s",
                                       v->val.lpnt[ i ], sep );
                count += T_fprintf( file_num, "%ld\n", v->val.lpnt[ i ] );
            }
            else
                for ( i = 0; i < v->len; i++ )
                    count += T_fprintf( file_num, "%ld\n", v->val.lpnt[ i ] );
            break;

        case FLOAT_ARR :
            if ( sep )
            {
                for ( i = 0; i < v->len - 1; i++ )
                    count += T_fprintf( file_num, "%#.9g%s",
                                        v->val.dpnt[ i ], sep );
                count += T_fprintf( file_num, "%#.9g\n", v->val.dpnt[ i ] );
            }
            else
                for ( i = 0; i < v->len; i++ )
                    count += T_fprintf( file_num, "%#.9g\n", v->val.dpnt[ i ] );
            break;

        case INT_REF : case FLOAT_REF :
            for ( i = 0; i < v->len; i++ )
            {
                if ( v->val.vptr[ i ] )
                    count += arr_save( sep, file_num, v->val.vptr[ i ] );
                if ( i != v->len - 1 && ! sep )
                    count += T_fprintf( file_num, "\n" );
            }
            break;

        default :
            break;
    }

    return count;
}


/*--------------------------------------------------------------------------*
 * Saves data to a file. If 'get_file()' hasn't been called yet it will be
 * called now - in this case the file opened this way is the only file to
 * be used and no file identifier is allowed as first argument to 'save()'.
 * This function is the formated save with the same meaning of the format
 * string as in 'print()'.
 * The function returns the number of chars it wrote to the file.
 *--------------------------------------------------------------------------*/

Var_T *
f_fsave( Var_T * v )
{
    /* Determine the file identifier */

    long file_num = get_save_file( &v );
    if ( file_num == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, 0L );

    if ( ! v )
    {
        print( WARN, "Missing arguments.\n" );
        return vars_push( INT_VAR, 0L );
    }

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Missing format string.\n" );
        THROW( EXCEPTION );
    }

    /* Rewrite the format string and check that arguments are ok, then print */

    f_format_check( v );

    return vars_push( INT_VAR, do_printf( file_num, v ) );
}


/*-------------------------------------------------------------------------*
 * This function gets called from f_fsave() to make from its format string
 * a format string that can be passed to fprintf()'s format string.
 *-------------------------------------------------------------------------*/

static
void
f_format_check( Var_T * v )
{
    /* Clean up the format string, especially replace the '#' characters with
       the appropriate conversion specifiers */

    Var_T * cv = v->next;
    for ( char * cp = v->val.sptr; *cp != '\0'; cp++ )
    {
        ptrdiff_t s2c;

        if ( *cp == '%' )                 /* '%' must be replaced by "%%" */
        {
            s2c = cp - v->val.sptr;
            v->val.sptr = T_realloc( v->val.sptr, strlen( v->val.sptr ) + 2 );
            cp = v->val.sptr + s2c;
            memmove( cp + 1, cp, strlen( cp ) + 1 );
            cp++;
            continue;
        }

        if ( *cp == '\\' )                /* replace escaped '#' characters */
        {
            int sc;

            for ( sc = 0; *cp == '\\'; sc++, cp++ )
                /* empty */ ;

            if ( sc & 1 && *cp =='#' )
                memmove( cp - 1, cp, strlen( cp ) + 1 );

            cp--;
            continue;
        }

        if ( *cp != '#' )                 /* keep "normal" characters */
            continue;

        if ( ! cv )                       /* '#' without variable to print */
        {
            print( FATAL, "Less data than format descriptors in format "
                   "string.\n" );
            THROW( EXCEPTION );
        }

        switch ( cv->type )               /* replace '#' by format specifier */
        {
            case INT_VAR :
                s2c = cp - v->val.sptr;
                v->val.sptr = T_realloc( v->val.sptr,
                                         strlen( v->val.sptr ) + 2 );
                cp = v->val.sptr + s2c;
                memmove( cp + 2, cp + 1, strlen( cp ) );
                memcpy( cp, "%d", 2 );
                cp++;
                break;

            case FLOAT_VAR :
                s2c = cp - v->val.sptr;
                v->val.sptr = T_realloc( v->val.sptr,
                                         strlen( v->val.sptr ) + 5 );
                cp = v->val.sptr + s2c;
                memmove( cp + 5, cp + 1, strlen( cp ) );
                memcpy( cp, "%#.9g", 5 );
                cp += 4;
                break;

            case STR_VAR :
                s2c = cp - v->val.sptr;
                v->val.sptr = T_realloc( v->val.sptr,
                                         strlen( v->val.sptr ) + 2 );
                cp = v->val.sptr + s2c;
                memmove( cp + 2, cp + 1, strlen( cp ) );
                memcpy( cp, "%s", 2 );
                handle_escape( cv->val.sptr );
                cp++;
                break;

            default :
                print( FATAL, "Function can only write numbers and strings "
                       "to a file.\n" );
                THROW( EXCEPTION );
        }

        cv = cv->next;
    }

    if ( cv )
    {
        print( SEVERE, "More data than format descriptors in format "
               "string.\n" );
        while ( ( cv = vars_pop( cv ) ) )
            /* empty */ ;
    }

    /* Finally replace the escape sequences in the format string */

    handle_escape( v->val.sptr );
}


/*-------------------------------------------------------------------------*
 * This function writes data to a file according to a format string, which
 * resembles strongly the format string that printf() and friends accepts.
 * The only things not supported (because they don't make sense here) are
 * printing of chars, unsigned quantities and pointers and pointers (i.e.
 * the 'c', 'o', 'x', 'X', 'u' and 'p' conversion specifiers cannot be
 * used) and no length specifiers (i.e. 'h', 'l' and 'L') are accepted.
 * Everything else should work, including all escape sequences (with the
 * exception of trigraph sequences).
 * The function returns the number of chars written to the file.
 *-------------------------------------------------------------------------*/

Var_T *
f_ffsave( Var_T * v )
{
    /* Determine the file identifier */

    long file_num = get_save_file( &v );
    if ( file_num == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, 0L );

    if ( ! v )
    {
        print( WARN, "Missing arguments.\n" );
        return vars_push( INT_VAR, 0L );
    }

    if ( v->type != STR_VAR )
    {
        print( FATAL, "Missing format string.\n" );
        THROW( EXCEPTION );
    }

    /* Check that format string and arguments are ok, then print */

    ff_format_check( v );

    return vars_push( INT_VAR, do_printf( file_num, v ) );
}


/*--------------------------------------------------------------------------*
 * Function is used by ff_save( ) to check that the format string is well-
 * formed and to find out if there are at least as many arguments as needed
 * by the format string (if there are more a warning is printed and the
 * superfluous arguments are discarded).
 *--------------------------------------------------------------------------*/

static
void
ff_format_check( Var_T * v )
{
    const char *sptr = v->val.sptr;
    Var_T *vptr = v->next;

    /* Replace escape characters in the format string */

    handle_escape( v->val.sptr );

    /* Loop over the format string to figure out if there are enough arguments
       for the format string and that the argument types are the ones expected
       by the conversion modifiers. */

    while ( 1 )
    {
        /* Skip over everything that's not a conversion specifier */

        for ( ; *sptr != '\0' && *sptr != '%'; sptr++ )
            /* empty */ ;

        if ( ! *sptr++ )
            break;

        if ( ! *sptr )
        {
            print( FATAL, "'%%' found at end of format string.\n" );
            THROW( EXCEPTION );
        }

        if ( *sptr == '%' )
            continue;

        /* First thing to be expected in a conversion specifier are flags */

        while (    *sptr == '-'
                || *sptr == '+'
                || *sptr == ' '
                || *sptr == '0'
                || *sptr == '#' )
        {
            sptr++;
            if ( ! *sptr )
            {
                print( FATAL, "End of format string within conversion "
                       "specifier.\n" );
                THROW( EXCEPTION );
            }
        }

        /* Next a minimum length and precision might follow, if one or both
           of these are given by a '*' we need an integer argument to
           specifiy the number to be used */

        if ( *sptr == '*' )
        {
            if ( vptr->type != INT_VAR )
            {
                print( FATAL, "Non-integer variable used as field length in "
                       "format string.\n" );
                THROW( EXCEPTION );
            }

            sptr++;
            vptr = vptr->next;
        }
        else if ( isdigit( ( unsigned char ) *sptr ) )
            while ( isdigit( ( unsigned char ) *++sptr ) )
                /* empty */ ;

        if ( ! *sptr )
        {
            print( FATAL, "End of format string within conversion "
                   "specifier.\n" );
            THROW( EXCEPTION );
        }

        if ( *sptr == '.' )
        {
            sptr++;

            if ( *sptr == '*' )
            {
                if ( ! vptr )
                {
                    print( FATAL, "Not enough arguments for format "
                           "string.\n" );
                    THROW( EXCEPTION );
                }

                if ( vptr->type != INT_VAR )
                {
                    print( FATAL, "Non-integer variable used as field "
                           "length in format string.\n" );
                    THROW( EXCEPTION );
                }

                sptr++;
                vptr = vptr->next;
            }
            else if ( isdigit( ( unsigned char ) *sptr ) )
                while ( isdigit( ( unsigned char ) *++sptr ) )
                    /* empty */ ;

            if ( ! *sptr )
            {
                print( FATAL, "End of format string within conversion "
                       "specifier.\n" );
                THROW( EXCEPTION );
            }
        }

        /* Now the conversion specifier has to follow, this can be either
           's', 'd', 'i', 'f', 'e', 'g', 'F', 'E', 'G', or 'n'. For 'n' no
           argument is needed because it just prints the number of character
           written up to the moment the 'n' is found in the format string. */

        if ( *sptr == 's' )
        {
            if ( ! vptr )
            {
                print( FATAL, "Not enough arguments for format string.\n" );
                THROW( EXCEPTION );
            }

            if ( vptr->type != STR_VAR )
            {
                print( FATAL, "Non-string variable found for string type "
                       "conversion specifier in format string.\n" );
                THROW( EXCEPTION );
            }

            handle_escape( vptr->val.sptr );
            sptr++;
            vptr = vptr->next;
            continue;
        }

        if ( *sptr == 'd' || *sptr == 'i' )
        {
            if ( ! vptr )
            {
                print( FATAL, "Not enough arguments for format string.\n" );
                THROW( EXCEPTION );
            }

            if ( vptr->type != INT_VAR )
            {
                print( WARN, "Non-integer variable found for integer type "
                       "conversion specifier in format string.\n" );
                THROW( EXCEPTION );
            }

            sptr++;
            vptr = vptr->next;
            continue;
        }

        if (    *sptr == 'f' || *sptr == 'e' || *sptr == 'g'
             || *sptr == 'F' || *sptr == 'E' || *sptr == 'G' )
        {
            if ( ! vptr )
            {
                print( FATAL, "Not enough arguments for format string.\n" );
                THROW( EXCEPTION );
            }

            if ( vptr->type != FLOAT_VAR )
            {
                print( WARN, "Non-floating point variable found for float "
                       "type conversion specifier in format string.\n" );
                THROW( EXCEPTION );
            }

            sptr++;
            vptr = vptr->next;
            continue;
        }

        print( FATAL, "Unknown conversion specifier '%c' found in format "
               "string.\n", *sptr );
        THROW( EXCEPTION );
    }

    /* Check for superfluous arguments, print a warning if there are any and
       discard them */

    if ( vptr )
    {
        print( SEVERE, "More arguments than conversion specifiers found in "
               "format.\n" );
        while ( ( vptr = vars_pop( vptr ) ) )
            /* empty */ ;
    }
}


/*-----------------------------------------------------------------------*
 * This function is called by f_fsave() and f_ffsave() to do the actual
 * printing. It loops over the format string and prints it argument by
 * argument (unfortunately, there's no portable way to create a variable
 * argument list, so it must be done this way). The function returns
 * the number of character written.
 *-----------------------------------------------------------------------*/

static
long
do_printf( long    file_num,
           Var_T * v )
{
    long volatile count = 0;
    char * volatile sptr = v->val.sptr;
    char * fmt_start = T_malloc( strlen( sptr ) + 2 );
    char * volatile fmt_end = fmt_start;
    strcpy( fmt_start, sptr );
    Var_T * volatile cv = v->next;

    TRY
    {
        /* Print everything up to the first conversion specifier */

        while ( *fmt_end != '\0' )
        {
            if ( *fmt_end == '%' )
            {
                if ( *( fmt_end + 1 ) == '%' )
                {
                    fmt_end += 2;
                    continue;
                }
                else
                    break;
            }

            fmt_end++;
        }

        char store = *fmt_end;
        *fmt_end = '\0';

        if ( fmt_start != fmt_end )
            count += T_fprintf( file_num, fmt_start );

        if ( ! store )                               /* already at the end ? */
        {
            TRY_SUCCESS;
            T_free( fmt_start );
            return count;
        }

        sptr += fmt_end - fmt_start;
        strcpy( fmt_start, sptr );
        fmt_end = fmt_start + 1;

        /* Now repeat printing, starting each time with a conversion specifier
           and ending just before the next one until the end of the format
           string has been reached */

        while ( true )
        {
            int need_vars = 0;              /* how many arguments are needed */
            int need_type = -1;             /* type of the argument to print */

            /* Skip over flags */

            while (    *fmt_end == '-'
                    || *fmt_end == '+'
                    || *fmt_end == ' '
                    || *fmt_end == '0'
                    || *fmt_end == '#' )
                fmt_end++;

            /* Deal with the minumum length and precision fields, checking
               if, due to a '*' an argument is needed */

            if ( *fmt_end == '*' )
            {
                need_vars++;
                fmt_end++;
            }
            else if ( isdigit( ( unsigned char ) *fmt_end ) )
                while ( isdigit( ( unsigned char ) *++fmt_end ) )
                    /* empty */ ;

            if ( *fmt_end == '.' )
            {
                if ( *++fmt_end == '*' )
                {
                    need_vars++;
                    fmt_end++;
                }
                else if ( isdigit( ( unsigned char ) *fmt_end ) )
                    while ( isdigit( ( unsigned char ) *++fmt_end ) )
                        /* empty */ ;
            }

            /* Find out about the type of the argument to be printed */

            if ( *fmt_end == 's' )
            {
                need_type = 0;       /* string */
                need_vars++;
            }

            if ( *fmt_end == 'd' || *fmt_end == 'i' )
            {
                /* Integers are always 'long' integers, so an 'l' must be
                   inserted in front of the 'd' or 'i' */

                memmove( fmt_end + 1, fmt_end, strlen( fmt_end ) + 1 );
                *fmt_end++ = 'l';

                need_type = 1;       /* integer */
                need_vars++;
            }

            if (    *fmt_end == 'f' || *fmt_end == 'e' || *fmt_end == 'g'
                 || *fmt_end == 'F' || *fmt_end == 'E' || *fmt_end == 'G' )
            {
                need_type = 2;       /* float */
                need_vars++;
            }

            /* Now get rest of string until the next conversion specifier or
               the end of the format string is reached */

            while ( *fmt_end != '\0' )
            {
                if ( *fmt_end == '%' )
                {
                    if ( *( fmt_end + 1 ) == '%' )
                    {
                        fmt_end += 2;
                        continue;
                    }
                    else
                        break;
                }

                fmt_end++;
            }

            store = *fmt_end;
            *fmt_end = '\0';

            /* Call the function doing the printing, supplying it with the
               necessary number of arguments */

            switch ( need_vars )
            {
                case 1 :
                    switch ( need_type )
                    {
                        case 0 : /* string */
                            count += T_fprintf( file_num, fmt_start,
                                                cv->val.sptr );
                            break;

                        case 1 : /* integer */
                            if ( cv->type == INT_VAR )
                                count += T_fprintf( file_num, fmt_start,
                                                    cv->val.lval );
                            else
                                count += T_fprintf( file_num, fmt_start,
                                                    lrnd( cv->val.dval ) );

                            break;

                        case 2 : /* double */
                            if ( cv->type == FLOAT_VAR )
                                count += T_fprintf( file_num, fmt_start,
                                                    cv->val.dval );
                            else
                                count += T_fprintf( file_num, fmt_start,
                                                    ( double ) cv->val.lval );
                            break;

                        default :
                            fsc2_impossible( );
                    }

                    cv = vars_pop( cv );
                    break;

                case 2 :
                    switch ( need_type )
                    {
                        case 0 : /* string */
                            count += T_fprintf( file_num, fmt_start,
                                                ( int ) cv->val.lval,
                                                cv->next->val.sptr );
                            break;

                        case 1 : /* integer */
                            if ( cv->next->type == INT_VAR )
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    cv->next->val.lval );
                            else
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    lrnd(
                                                        cv->next->val.dval ) );
                            break;

                        case 2 : /* double */
                            if ( cv->next->type == FLOAT_VAR )
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    cv->next->val.dval );
                            else
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    ( long )
                                                          cv->next->val.lval );
                            break;

                        default :
                            fsc2_impossible( );
                    }

                    cv = vars_pop( vars_pop( cv ) );
                    break;

                case 3 :
                    switch ( need_type )
                    {
                        case 0 : /* string */
                            count += T_fprintf( file_num, fmt_start,
                                                ( int ) cv->val.lval,
                                                ( int ) cv->next->val.lval,
                                                cv->next->next->val.sptr );
                            break;

                        case 1 : /* integer */
                            if ( cv->next->next->type == INT_VAR )
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    ( int ) cv->next->val.lval,
                                                    cv->next->next->val.lval );
                            else
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    ( int ) cv->next->val.lval,
                                                    lrnd(
                                                    cv->next->next->val.dval )
                                                   );
                            break;

                        case 2 : /* double */
                            if ( cv->next->next->type == FLOAT_VAR )
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    ( int ) cv->next->val.lval,
                                                    cv->next->next->val.dval );
                            else
                                count += T_fprintf( file_num, fmt_start,
                                                    ( int ) cv->val.lval,
                                                    ( int ) cv->next->val.lval,
                                                    ( long )
                                                    cv->next->next->val.lval );
                            break;

                        default :
                            fsc2_impossible( );
                    }

                    cv = vars_pop( vars_pop( vars_pop( cv ) ) );
                    break;

                default :
                    fsc2_impossible( );
            }

            if ( ! store )                                 /* end reached ? */
                break;

            sptr += fmt_end - fmt_start - ( need_type == 1 ? 1 : 0 );
            strcpy( fmt_start, sptr );
            fmt_end = fmt_start + 1;
        }

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( fmt_start );
        RETHROW;
    }

    T_free( fmt_start );
    return count;
}


/*-------------------------------------------------------------------------*
 * Saves the EDL program to a file. If 'get_file()' hasn't been called yet
 * it will be called now - in this case the file opened this way is the
 * only file to be used and no file identifier is allowed as first argu-
 * ment to 'save()'.
 * Beside the file identifier the other (optional) parameter is a string
 * that gets prepended to each line of the EDL program (i.e. a comment
 * character).
 *-------------------------------------------------------------------------*/

Var_T *
f_save_p( Var_T * v )
{
    /* Determine the file identifier */

    long file_num = get_save_file( &v );
    if ( file_num == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, 0L );

    if ( v )
        vars_check( v, STR_VAR );

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, 1L );

    if ( ! print_browser( 0, file_num, v ? v->val.sptr : "" ) )
        THROW( EXCEPTION );

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------------------*
 * Saves the content of the output window to a file. If 'get_file()' hasn't
 * been called yet it will be called now - in this case the file opened
 * this way is the only file to be used and no file identifier is allowed
 * as first argument to 'save()'.
 * Beside the file identifier the other (optional) parameter is a string
 * that gets prepended to each line of the output (i.e. a comment char).
 *--------------------------------------------------------------------------*/

Var_T *
f_save_o( Var_T * v )
{
    /* Determine the file identifier */

    long file_num = get_save_file( &v );
    if ( file_num == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, 0L );

    if ( v )
        vars_check( v, STR_VAR );

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, 1L );

    if ( ! print_browser( 1, file_num, v ? v->val.sptr : "" ) )
        THROW( EXCEPTION );

    return vars_push( INT_VAR, 1L );
}


/*---------------------------------------------------------------------*
 * Writes the content of the program or the error browser into a file.
 * Input parameter:
 * 1. 0: writes program browser, 1: error browser
 * 2. File identifier
 * 3. Comment string to prepend to each line
 *---------------------------------------------------------------------*/

static
long
print_browser( int          browser,
               int          fid,
               const char * comment )
{
    writer( browser ==  0 ? C_PROG : C_OUTPUT );
    if ( ! comment )
        comment = "";

    long count = T_fprintf( fid, "%s\n", comment );

    while ( true )
    {
        char *line;
        reader( &line );

        char * lp = line;
        if ( line )
        {
            if ( browser == 0 )
            {
                while ( *lp++ != ':' )
                    /* empty */ ;

                char * cp = lp++;
                while ( *cp == ' ' || *cp == '\t' )
                    cp++;
                if ( ! strncmp( cp, "#INCLUDE", 8 ) )
                {
                    count += T_fprintf( fid, "%s// %s\n", comment, lp );
                    count += print_include( fid, cp + 8, comment,
                                            EDL.files->name );
                    continue;
                }
            }
            else if ( *line == '@' )
            {
                lp++;
                while ( *lp++ != 'f' )
                    /* empty */ ;
            }

            count += T_fprintf( fid, "%s%s\n", comment, lp );
        }
        else
            break;
    }

    count += T_fprintf( fid, "%s\n", comment );

    return count;
}


/*------------------------------------------------------*
 * Function for (recursively if necessary) printing out
 * a file specified in an #INCLUDE directive.
 *------------------------------------------------------*/

#define MAX_INCLUDE_DEPTH 16         /* should be identical to the same
                                        value in fsc2_clean.l */
#define PRINT_BUF_SIZE 1025

static
long
print_include( int          fid,
               char       * volatile cp,
               const char * volatile   comment,
               const char  * cur_file )
{
    static long count;
    static int level;

    if ( ! strcmp( cur_file, EDL.files->name ) )
    {
        level = 0;
        count = 0L;
    }
    else level++;

    if ( level > MAX_INCLUDE_DEPTH )
    {
        level--;
        return count;
    }

    /* Skip leading white space */

    while ( *cp == ' ' || *cp == '\t' )
        cp++;

    /* Try to extract the file name */

    char * ep = cp++;
    char volatile delim = *ep++;

    if ( delim != '"' && delim != '<' )
    {
        level--;
        return count;
    }
    else if ( delim == '<' )
        delim = '>';

    while ( *ep != '\0' && *ep != delim )
        ep++;

    /* Give up when no file name could be extracted */

    if ( ! *ep || ( delim != '"' && delim != '>' ) )
    {
        level--;
        return count;
    }

    *ep = '\0';

    /* Now adorn the file name with the required path. If the include file is
       enclosed in '<' and '>' we try to use the system wide default include
       directory (if it is defined, otherwise we give up). If its included
       in double quotes we check if the name starts with a '/', indicating
       an absolute path, in which case we take it. If it starts with "~/"
       the '~' gets replaced by the users home directory. If we find only
       a name or a relative path we prepend the name of the directory the
       EDL file is from in which we found the '#INCLUDE' directive. */

    char * volatile file_name = NULL;

    TRY
    {
        if ( delim == '"' )
        {
            if ( *cp == '/' )
                file_name = T_strdup( cp );
            else if ( *cp == '~' && cp[ 1 ] == '/' )
            {
                struct passwd *pwe = getpwuid( getuid( ) );
                file_name = T_malloc( strlen( pwe->pw_dir ) + strlen( cp ) );
                strcpy( file_name, pwe->pw_dir );
                strcat( file_name, cp + 1 );
            }
            else
            {
                file_name = T_strdup( cur_file );
                *strrchr( file_name, '/' ) = '\0';
                file_name = T_realloc(   file_name, strlen( file_name )
                                       + strlen( cp ) + 2 );
                strcat( file_name, "/" );
                strcat( file_name, cp );
            }

        }
        else
        {
#if defined DEF_INCL_DIR
            file_name = T_malloc( strlen( cp ) + strlen( DEF_INCL_DIR ) + 3 );
            strcpy( file_name, DEF_INCL_DIR );
            if ( file_name[ strlen( file_name ) - 1 ] != '/' )
                strcat( file_name, "/" );
            strcat( file_name, cp );
#else
            level--;
            return count;
#endif
        }
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        level--;
        T_free( file_name );
        return count;
    }

    /* Try to open the include file, give up on failure */

    FILE * finc = fopen( file_name, "r" );
    if ( ! finc )
    {
        level--;
        T_free( file_name );
        return count;
    }

    /* Now print the include file, taking care of include files within the
       include file */

    TRY
    {
        count = T_fprintf( fid, "%s// --- %c%s%c starts here --------------\n",
                           comment, delim == '"' ? '"' : '<', cp, delim );

        char buf[ PRINT_BUF_SIZE ];
        while ( fgets( buf, PRINT_BUF_SIZE, finc ) )
        {
            count += T_fprintf( fid, "%s%s", comment, buf );

            ep = buf;
            while ( *ep && ( *ep == ' ' || *ep == '\t' ) )
                ep++;

            if ( ! strncmp( ep, "#INCLUDE", 8 ) )
                count += print_include( fid, ep + 8, comment, file_name );
        }

        count = T_fprintf( fid, "%s// --- %c%s%c ends here ----------------\n",
                           comment, delim == '"' ? '"' : '<', cp, delim );
        fclose( finc );

        TRY_SUCCESS;
    }
    OTHERWISE
    {
        level--;
        fclose( finc );
        T_free( file_name );
        RETHROW;
    }

    level--;
    T_free( file_name );
    return count;
}


/*---------------------------------------------------------------------*
 * Writes a comment into a file.
 * Arguments:
 * 1. If first argument is a number it's treated as a file identifier.
 * 2. It follows a comment string to prepend to each line of text
 * 3. A text to appear in the editor
 * 4. The label for the editor
 *---------------------------------------------------------------------*/

Var_T *
f_save_c( Var_T * v )
{
    const char *cc = NULL;
    char *c = NULL,
         *l = NULL,
         *r = NULL,
         *cl,
         *nl;

    /* Determine the file identifier */

    long file_num = get_save_file( &v );
    if ( file_num == FILE_NUMBER_NOT_OPEN )
        return vars_push( INT_VAR, 0L );

    if ( Fsc2_Internals.mode == TEST )
        return vars_push( INT_VAR, 1L );

    /* Try to get the comment chars to prepend to each line */

    if ( v )
    {
        vars_check( v, STR_VAR );
        cc = v->val.sptr;
        v = v->next;
    }

    /* Try to get the predefined content of the editor */

    if ( v )
    {
        vars_check( v, STR_VAR );
        c = v->val.sptr;
        correct_line_breaks( c );
        v = v->next;
    }

    /* Try to get a label string for the editor */

    if ( v )
    {
        vars_check( v, STR_VAR );
        l = v->val.sptr;
    }

    /* Show the comment editor and get the returned contents (just one string
       with embedded newline chars) */

    if (    ! ( Fsc2_Internals.cmdline_flags & DO_CHECK )
         && ! ( Fsc2_Internals.cmdline_flags & BATCH_MODE ) )
         r = T_strdup( show_input( c, l ) );

    if ( ! r )
        return vars_push( INT_VAR, 1L );

    if ( ! *r )
    {
        T_free( r );
        return vars_push( INT_VAR, 1L );
    }

    cl = r;
    if ( ! cc )
        cc = "";
    T_fprintf( file_num, "%s\n", cc );

    while ( cl )
    {
        nl = strchr( cl, '\n' );
        if ( nl )
            *nl++ = '\0';
        T_fprintf( file_num, "%s%s\n", cc, cl );
        cl = nl;
    }

    if ( cc )
        T_fprintf( file_num, "%s\n", cc );

    T_free( r );

    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------------*
 * Function that does all the writing to files. It does lots of tests
 * to make sure that really everything got written to the file and
 * tries to handle situations gracefully where there isn't enough
 * space left on a disk by asking for a replacement file and copying
 * everything already written to the file on the full disk into the
 * replacement file. The function returns the number of chars written
 * to the file (but not the copied bytes in case a replacement file
 * had to be used).
 *--------------------------------------------------------------------*/

#define BUFFER_SIZE_GUESS 128       /* guess for number of characters needed */

static
long
T_fprintf( long         fn,
           const char * fmt,
           ... )
{
    long size = BUFFER_SIZE_GUESS;
    char initial_buffer[ BUFFER_SIZE_GUESS ];
    char *p = initial_buffer;
    long file_num = fn;
    File_List_T *fl = EDL.File_List  + file_num - FILE_NUMBER_OFFSET;

    /* If the file has been closed because of insufficient space just don't
       print */

    if ( Fsc2_Internals.mode != TEST )
    {
        if ( file_num == FILE_NUMBER_NOT_OPEN )
            return 0;

        if (    file_num < FILE_NUMBER_OFFSET
             || file_num >= EDL.File_List_Len + FILE_NUMBER_OFFSET )
        {
            print( FATAL, "Invalid file handle.\n" );
            THROW( EXCEPTION );
        }

        if (    ( ! fl->gzip && ! fl->fp )
             || (   fl->gzip && ! fl->gp ) )
            return 0;
    }

    file_num -= FILE_NUMBER_OFFSET;

    /* First we've got to find out how many characters we need to write out.
       We start by trying to write to a fixed size memory buffer. If the
       required string is longer we try allocating a longer buffer (where
       newer libc versions already tell us how memory much is needed while
       older don't, so we may have to retry several times with increased buffer
       sizes) and write to that. To speed things up we start with a buffer
       that is local to the function and only start calling memory allocation
       functions if the original buffer isn't large enough. */

    long to_write;

    while ( true )
    {
        /* Try to print into the allocated space */

        va_list ap;
        va_start( ap, fmt );
        to_write = vsnprintf( p, size, fmt, ap );
        va_end( ap );

        /* If that worked, try to write out the string */

        if ( to_write > -1 && size > to_write )
            break;

        /* Else try again with more space */

        if ( to_write > -1 )         /* glibc 2.1 */
            size = to_write + 1;     /* precisely what is needed */
        else                         /* glibc 2.0 */
        {
            if ( size < INT_MAX / 2 )
                size *= 2;           /* twice the old size */
            else
            {
                show_message( "String to be written is too long." );
                T_free( p );
                THROW( EXCEPTION );
            }
        }

        if ( p == initial_buffer )
            p = NULL;
        p = T_realloc_or_free( p, size );
    }

    if ( Fsc2_Internals.mode == TEST )
    {
        if ( p != initial_buffer )
            T_free( p );
        return to_write;
    }

    /* Now we try to write the string to the file */

    long count;
    long written = 0;

 get_repeat_write:

    count = fl->gzip ? gzwrite( fl->gp, p + written, to_write - written)
                     : ( ssize_t ) fwrite( p + written, 1, to_write - written,
                                           fl->fp );

    if ( count == to_write - written )
    {
        if ( p != initial_buffer )
            T_free( p );
        return count + written;
    }

    /* If less characters than required where written we reduce 'to_write' to
       the number of characters that still need to be written out. */

    if ( count > 0 )
        written  += count;

    /* We can't do anything to save the day when writing failed for stdout or
       stderr, all we can do is print out a warning and return... */

    if ( file_num == 0 || file_num == 1 )
    {
        print( SEVERE, "Can't write to std%s, if it's redirected to a "
               "file make sure there's enough space on the disk.\n",
               file_num == 0 ? "out" : "err" );
        if ( p != initial_buffer )
            T_free( p );
        return written;
    }

    /* Couldn't write as many bytes as needed, disk is probably full.
       Ask the user to elete some files and try again. */

    char * mess = get_string( "Disk full while writing to file\n%s\n"
                              "Please delete some files.", fl->name );
    show_message( mess );
    T_free( mess );

    /* If the user deleted the file we're currently writing to delete it */

    struct stat stat_buf;
    if ( stat( fl->name, &stat_buf ) == -1 )
    {
        if ( p != initial_buffer )
            T_free( p );
        T_free( fl->name );
        fl->name = NULL;
        if ( ! fl->gzip && fl->fp )
            fclose( fl->fp );
        else if ( fl->gzip && fl->gp )
            gzclose( fl->gp );
        fl->fp = NULL;
        fl->gp = NULL;
        return written;
    }

    goto get_repeat_write;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
