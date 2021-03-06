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


static int fsc2_simplex_is_minimum( int      n,
                                    double * y,
                                    double   epsilon );
static ssize_t do_read( int    fd,
                        char * ptr );


/*------------------------------------------------------------------*
 * Function expects a format string as printf() and arguments which
 * must correspond to the given format string and returns a string
 * of the right length into which the arguments are written. The
 * caller of the function is responsible for free-ing the string.
 * -> 1. printf()-type format string
 *    2. As many arguments as there are conversion specifiers etc.
 *       in the format string.
 * <- Pointer to character array of exactly the right length into
 *    which the string characterized by the format string has been
 *    written. On failure, i.e. if there is not enough space, the
 *    function throws an OUT_OF_MEMORY exception.
 *------------------------------------------------------------------*/

#define GET_STRING_TRY_LENGTH 128

char *
get_string( const char * restrict fmt,
            ... )
{
    char * str = NULL;
    size_t len = GET_STRING_TRY_LENGTH;
    int wr;

    va_list ap;
    va_start( ap, fmt );

    while ( true )
    {
        str = T_realloc_or_free( str, len );

        va_list ap_cpy;
        va_copy( ap_cpy, ap );
        wr = vsnprintf( str, len, fmt, ap_cpy );
        va_end( ap_cpy );

        if ( wr < 0 )         /* indicates not enough space with older glibs */
        {
            len *= 2;
            continue;
        }

        if ( ( size_t ) wr + 1 > len )   /* newer glibs return the number of */
        {                                /* chars needed, not counting the   */
            len = wr + 1;                /* trailing '\0'                    */
            continue;
        }

        break;
    }

    va_end( ap );

    /* Trim the string down to the number of required characters */

    if ( ( size_t ) wr + 1 < len )
        str = T_realloc_or_free( str, wr + 1 );

    return str;
}


/*--------------------------------------------------------------*
 * Converts all upper case characters in a string to lower case
 * (in place, i.e. the string itself is changed, not a copy of
 * the string).
 *--------------------------------------------------------------*/

char *
string_to_lower( char * str )
{
    char *ptr;


    if ( ! str )
        return NULL;
    for ( ptr = str; *ptr; ptr++ )
        if ( isupper( ( unsigned char ) *ptr ) )
            *ptr = ( char ) tolower( ( unsigned char ) *ptr );

    return str;
}


/*---------------------------------------------------*
 * This routine returns a copy of a piece of memory.
 * On failure an OUT_OF_MEMORY exception is thrown.
 *---------------------------------------------------*/

void *
get_memcpy( const void * array,
            size_t       size )
{
    void * new_mem = T_malloc( size );
    memcpy( new_mem, array, size );
    return new_mem;
}


/*----------------------------------------------------------------------*
 * Function replaces all occurrences of the character combination "\\n"
 * in a string by the line break character '\n'. This is done in place,
 * i.e. the string passed to the function is changed, not a copy. So,
 * never call it with a char array defined as const.
 *----------------------------------------------------------------------*/

char *
correct_line_breaks( char * str )
{
    char * p = str;
    while ( ( p = strstr( p, "\\n" ) ) )
    {
        *p++ = '\n';
        memmove( p, p + 1, strlen( p ) );
    }

    return str;
}


/*--------------------------------------------------------------------*
 * strip_path() returns a pointer to the basename of a path, i.e. for
 * "/usr/bin/emacs" a pointer to "emacs" in the string is returned.
 *--------------------------------------------------------------------*/

const char *
strip_path( const char * path )
{
    char * cp;
    if ( ! ( cp = strrchr( path, '/' ) ) )
        return path;
    else
        return ++cp;
}


/*-----------------------------------------------------------------*
 * Function returns the string "/" if the string passed to it does
 * not end with a slash, otherwise it returns the empty string "".
 *-----------------------------------------------------------------*/


const char *
slash( const char * path )
{
    return path[ strlen( path ) - 1 ] != '/' ? "/" : "";
}


/*-----------------------------------------------------------------*
 * get_file_length() returns the number of lines in a file as well
 * as the number of digits in the number of lines.
 * ->
 *   * name of file
 *   * pointer for returning number of digits in number of lines
 * <-
 *   * number of lines or -1: failed to get file descriptor
 *-----------------------------------------------------------------*/

long
get_file_length( FILE * restrict fp,
                 int  * restrict len )
{
    int fd = fileno( fp );
    if ( fd == -1 )
         return -1;

    ssize_t bytes_read;
    char buffer[ 4096 ];
    long lines = 0;
    bool is_char = false;

    while ( ( bytes_read = read( fd, buffer, 4096 ) ) > 0 )
    {
        char * cur = buffer;
        char * end = buffer + bytes_read;

        while ( cur < end )
        {
            if ( *cur++ == '\n' )
            {
                lines++;
                is_char = false;
            }
            else
            {
                is_char = true;
                while ( cur < end && *cur != '\n' )
                    cur++;
            }
        }
    }

    /* If the last line does not end with a '\n' increment line count */

    if ( is_char )
        lines++;

    lseek( fd, 0, SEEK_SET );

    /* Count number of digits of number of lines */

    *len = 1;
    for ( long i = lines; ( i /= 10 ) > 0; ++*len )
        /* empty */ ;

    return lines;
}


/*--------------------------------------------------------------------*
 * This function is used all over the program to print error messages.
 * Its first argument is the severity of the error, an integer in the
 * range between 0 (FATAL) and 3 (NO_ERROR), see also global.h for
 * abbreviations. If the second argument, a boolean value, is set,
 * the name of the currently loaded EDL program and the line number
 * we are at is prepended to the output string. The next argument is
 * a format string having the same syntax as the format string for
 * functions like printf(). Then an unspecified number of arguments
 * may follow (which should correspond to the conversion specifiers
 * in the format string).
 * When the program is running with graphics the output goes to the
 * error browser, otherwise to stderr. The maximum line length to be
 * output is FL_BROWSER_LINELENGTH, which was defined to 2048 (at
 * least the last time I checked) in /usr/include/forms.h. Usually,
 * one can't use the complete length, because of the prepended file
 * name, and a few bytes are also used for other purposes. So better
 * keep the expected line length a bit lower (if the line would get
 * too long it gets truncated).
 *--------------------------------------------------------------------*/

void
eprint( int          severity,
        bool         print_fl,
        const char * fmt,
        ... )
{
    if ( severity != NO_ERROR )
        EDL.compilation.error[ severity ] += 1;

    if ( ! ( Fsc2_Internals.cmdline_flags & ( TEST_ONLY | NO_GUI_RUN ) ) )
    {
        char buffer[ FL_BROWSER_LINELENGTH + 1 ];
        char * cp = buffer;
        int space_left = FL_BROWSER_LINELENGTH;

        if ( severity == FATAL )
        {
            strcpy( buffer, "@C1@f" );
            cp += 5;
            space_left -= 5;
        }
        else if ( severity == SEVERE )
        {
            strcpy( buffer, "@C4@f" );
            cp += 5;
            space_left -= 5;
        }
        else if ( severity == WARN )
        {
            strcpy( buffer, "@C2@f" );
            cp += 5;
            space_left -= 5;
        }

        if ( space_left > 0 && print_fl && EDL.Fname )
        {
            int count = snprintf( cp, ( size_t ) space_left, "%s:%ld: ",
                                  EDL.Fname, EDL.Lc );
            space_left -= count;
            cp += count;
        }

        if ( space_left <= 0 )
            strcpy( buffer, "Message too long to be displayed!\n" );
        else
        {
            va_list ap;
            va_start( ap, fmt );
            vsnprintf( cp, ( size_t ) space_left, fmt, ap );
            va_end( ap );
        }

        if ( Fsc2_Internals.I_am == PARENT )
        {
            fl_freeze_form( GUI.main_form->error_browser->form );
            fl_addto_browser_chars( GUI.main_form->error_browser, buffer );

            fl_set_browser_topline( GUI.main_form->error_browser,
                   fl_get_browser_maxline( GUI.main_form->error_browser )
                   - fl_get_browser_screenlines( GUI.main_form->error_browser )
                   + 1 );

            fl_unfreeze_form( GUI.main_form->error_browser->form );

            if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
                fprintf( severity == NO_ERROR ? stdout : stderr,
                         "%s", buffer );
        }
        else
            writer( C_EPRINT, buffer );
    }
    else                               /* simple test run ? */
    {
        if ( severity != NO_ERROR )
            fprintf( stderr, "%c ", severity[ "FSW" ] );

        if ( print_fl && EDL.Fname )
            fprintf( severity == NO_ERROR ? stdout : stderr,
                     "%s:%ld: ", EDL.Fname, EDL.Lc );

        va_list ap;
        va_start( ap, fmt );
        vfprintf( severity == NO_ERROR ? stdout : stderr, fmt, ap );
        va_end( ap );
        if ( severity == NO_ERROR )
            fflush( stdout );
    }
}


/*------------------------------------------------------------------------*
 * This a somewhat simplified version of the previous function, eprint()
 * mainly for writers of modules. The only argument beside the usual ones
 * one would pass to printf() and friends is the severity of the error.
 * Everything else (i.e. the decision if to prepend a file name, a line
 * number or a function name) is dealt with automatically.
 *------------------------------------------------------------------------*/

void
print( int          severity,
       const char * fmt,
       ... )
{
    if ( severity != NO_ERROR )
        EDL.compilation.error[ severity ] += 1;

    if ( ! ( Fsc2_Internals.cmdline_flags & ( TEST_ONLY | NO_GUI_RUN ) ) )
    {
        char buffer[ FL_BROWSER_LINELENGTH + 1 ];
        char * cp = buffer;
        int space_left = FL_BROWSER_LINELENGTH;

        if ( severity == FATAL )
        {
            strcpy( buffer, "@C1@f" );
            cp += 5;
            space_left -= 5;
        }
        else if ( severity == SEVERE )
        {
            strcpy( buffer, "@C4@f" );
            cp += 5;
            space_left -= 5;
        }
        else if ( severity == WARN )
        {
            strcpy( buffer, "@C2@f" );
            cp += 5;
            space_left -= 5;
        }

        /* Print EDL file name and line number unless we're running a hook
           function */

        if ( space_left > 0 && ! Fsc2_Internals.in_hook && EDL.Fname )
        {
            int count = snprintf( cp, ( size_t ) space_left, "%s:%ld: ",
                                  EDL.Fname, EDL.Lc );
            space_left -= count;
            cp += count;
        }

        if ( space_left <= 0 )
            strcpy( buffer, "Message too long to be displayed!\n" );
        else if ( EDL.Call_Stack )
        {
            if ( ! EDL.Call_Stack->f )
            {
                if ( EDL.Call_Stack->dev_name )
                {
                    int count = snprintf( cp, ( size_t ) space_left, "%s: ",
                                          EDL.Call_Stack->dev_name );
                    space_left -= count;
                    cp += count;
                }
            }
            else
            {
                if ( EDL.Call_Stack->f->device )
                {
                    int count = snprintf( cp, ( size_t ) space_left, "%s: ",
                                          EDL.Call_Stack->f->device->name );
                    space_left -= count;
                    cp += count;
                }

                if ( EDL.Call_Stack->f->name )
                {
                    int count = snprintf( cp, ( size_t ) space_left, "%s(): ",
                                          EDL.Call_Stack->f->name );
                    space_left -= count;
                    cp += count;
                }
            }
        }

        if ( space_left <= 0 )
            strcpy( buffer, "Message too long to be printed!\n" );
        else
        {
            va_list ap;
            va_start( ap, fmt );
            vsnprintf( cp, ( size_t ) space_left, fmt, ap );
            va_end( ap );
        }

        if ( Fsc2_Internals.I_am == PARENT )
        {
            fl_freeze_form( GUI.main_form->error_browser->form );
            fl_addto_browser_chars( GUI.main_form->error_browser, buffer );

            fl_set_browser_topline( GUI.main_form->error_browser,
                   fl_get_browser_maxline( GUI.main_form->error_browser )
                   - fl_get_browser_screenlines( GUI.main_form->error_browser )
                   + 1 );

            fl_unfreeze_form( GUI.main_form->error_browser->form );

            if ( Fsc2_Internals.cmdline_flags & DO_CHECK )
                fprintf( severity == NO_ERROR ? stdout : stderr,
                         "%s", buffer );
        }
        else
            writer( C_EPRINT, buffer );
    }
    else                               /* simple test run ? */
    {
        if ( severity != NO_ERROR )
            fprintf( stderr, "%c ", severity[ "FSW" ] );

        if ( ! Fsc2_Internals.in_hook && EDL.Fname )
            fprintf( severity == NO_ERROR ? stdout : stderr,
                     "%s:%ld: ", EDL.Fname, EDL.Lc );

        if ( EDL.Call_Stack )
        {
            if ( ! EDL.Call_Stack->f )
            {
                if ( EDL.Call_Stack->dev_name )
                    fprintf( severity == NO_ERROR ? stdout : stderr,
                             "%s: ", EDL.Call_Stack->dev_name );
            }
            else
            {
                if ( EDL.Call_Stack->f->device )
                    fprintf( severity == NO_ERROR ? stdout : stderr,
                             "%s: ", EDL.Call_Stack->f->device->name );

                if ( EDL.Call_Stack->f->name )
                    fprintf( severity == NO_ERROR ? stdout : stderr,
                             "%s(): ", EDL.Call_Stack->f->name );
            }
        }

        va_list ap;
        va_start( ap, fmt );
        vfprintf( severity == NO_ERROR ? stdout : stderr, fmt, ap );
        va_end( ap );
        if ( severity == NO_ERROR )
            fflush( stdout );
    }
}


/*---------------------------------------------------------------------*
 * The program starts with the EUID and EGID set to the ones of fsc2,
 * but these privileges get dropped immediately. Only for some special
 * actions (like dealing with shared memory and lock and log files)
 * this function is called to change the EUID and EGID to the one of
 * fsc2.
 *---------------------------------------------------------------------*/

void
raise_permissions( void )
{
    if (    seteuid( Fsc2_Internals.EUID )
         || setegid( Fsc2_Internals.EGID ) )
    {
        print( FATAL, "Failed to raise permissions as required.\n" );
        THROW( EXCEPTION );
    }
}


/*---------------------------------------------------------------------*
 * This function sets the EUID and EGID to the one of the user running
 * the program.
 *---------------------------------------------------------------------*/

void
lower_permissions( void )
{
    if (    seteuid( getuid( ) )
         || setegid( getgid( ) ) )
    {
        print( FATAL, "Failed to lower permissions as required.\n" );
        THROW( EXCEPTION );
    }
}


/*-----------------------------------------------------------------*
 * Function replaces escape sequences in a string by the character
 * it stands for - all C type escape sequences are supported.
 *-----------------------------------------------------------------*/

char *
handle_escape( char * str )
{
    fsc2_assert( str != NULL );

    char * cp = str;
    while ( ( cp = strchr( cp, '\\' ) ) )
    {
        size_t esc_len;

        switch ( *( cp + 1 ) )
        {
            case '\0' :
                print( FATAL, "End of string directly following escape "
                       "character '\\'.\n" );
                THROW( EXCEPTION );
                break;

            case 'a' :
                *cp++ = '\a';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 'b' :
                *cp++ = '\b';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 'f' :
                *cp++ = '\f';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 'n' :
                *cp++ = '\n';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 'r' :
                *cp++ = '\r';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 't' :
                *cp++ = '\t';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 'v' :
                *cp++ = '\v';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case '\\' :
                *cp++ = '\\';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case '\?' :
                *cp++ = '\?';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case '\'' :
                *cp++ = '\'';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case '\"' :
                *cp++ = '\"';
                memmove( cp, cp + 1, strlen( cp ) );
                break;

            case 'x' :
                if ( ! isxdigit( ( unsigned char ) *( cp + 2 ) ) )
                {
                    print( FATAL, "'\\x' with no following hex digits "
                           "in string.\n" );
                    THROW( EXCEPTION );
                }
                esc_len = 1;
                *cp = toupper( ( unsigned char ) *( cp + 2 ) )
                      - ( isdigit( ( unsigned char )*( cp + 2 ) ) ?
                          '0' : ( 'A' - 10 ) );

                if ( isxdigit( ( unsigned char ) *( cp + 3 ) ) )
                {
                    esc_len++;
                    *cp = *cp * 16
                          + toupper( ( unsigned char ) *( cp + 3 ) )
                          - ( isdigit( ( unsigned char ) *( cp + 3 ) ) ?
                              '0' : ( 'A' - 10 ) );
                }

                cp++;
                memmove( cp, cp + 1 + esc_len, strlen( cp + esc_len ) );
                break;

            default :
                if ( *( cp + 1 ) < '0' || *( cp + 1 ) > '7' )
                {
                    print( FATAL, "Unknown escape sequence '\\%c' in "
                           "string.\n", *( cp + 1 ) );
                    THROW( EXCEPTION );
                }

                *cp = *( cp + 1 ) - '0';
                esc_len = 1;

                if ( *( cp + 2 ) >= '0' && *( cp + 2 ) <= '7' )
                {
                    *cp = *cp * 8 + *( cp + 2 ) - '0';
                    esc_len++;

                    if (    *( cp + 3 ) >= '0'
                         && *( cp + 3 ) <= '7'
                         && *cp <= 0x1F )
                    {
                        *cp = *cp * 8 + *( cp + 3 ) - '0';
                        esc_len++;
                    }
                }

                cp++;
                memmove( cp, cp + esc_len, strlen( cp + esc_len ) + 1 );
                break;
        }
    }

    return str;
}


/*-------------------------------------------------------------------------*
 * This routine takes the input file and feeds it to 'fsc2_clean' which is
 * running as a child process. The output of fsc2_clean gets written to a
 * pipe for which an immediately readable stream (the immediately bit is
 * important because otherwise the lexer just sees an EOF instead of the
 * output of fsc2_clean...) is returned by the function (or NULL instead
 * on errors within the parent process).
 *-------------------------------------------------------------------------*/

FILE *
filter_edl( const char * restrict name,
            FILE       * restrict fp,
            int        * restrict serr )
{
    /* The first set of pipes is needed to read the output of 'fsc2_clean' */

    int pd[ 2 ];
    if ( pipe( pd ) == -1 )
    {
        if ( errno == EMFILE || errno == ENFILE )
            print( FATAL, "Starting the test procedure failed, running out "
                   "of system resources.\n" );
        return NULL;
    }

    int ed[ 2 ];
    if ( pipe( ed ) == -1 )
    {
        close( pd[ 0 ] );
        close( pd[ 1 ] );
        if ( errno == EMFILE || errno == ENFILE )
            print( FATAL, "Starting the test procedure failed, running out "
                   "of system resources.\n" );
        return NULL;
    }

    /* The second set of pipes is only needed to make sure that the parent
       runs first by having the parent write something to the pipe and the
       child process waiting for it. This is required because otherwise it
       sometimes (especially for short EDL scripts) happens that the whole
       child process is already finished and dead before the parent even
       knows about its PID. In this case the signal handler does not know
       about the child aleady running and would thus not store the childs
       return value which we still need to figure out if 'fsc2_clean' did
       exit successfully. */

    int pdt[ 2 ];
    if ( pipe( pdt ) == -1 )
    {
        close( pd[ 0 ] );
        close( pd[ 1 ] );
        close( ed[ 0 ] );
        close( ed[ 1 ] );
        if ( errno == EMFILE || errno == ENFILE )
            print( FATAL, "Starting the test procedure failed, running out "
                   "of system resources.\n" );
        return NULL;
    }

    if ( ( Fsc2_Internals.fsc2_clean_pid = fork( ) ) < 0 )
    {
        close( pd[ 0 ] );
        close( pd[ 1 ] );
        close( ed[ 0 ] );
        close( ed[ 1 ] );
        close( pdt[ 0 ] );
        close( pdt[ 1 ] );
        if ( errno == ENOMEM || errno == EAGAIN )
            print( FATAL, "Starting the test procedure failed, running out "
                   "of system resources.\n" );
        return NULL;
    }

    /* Here's the childs code */

    if ( Fsc2_Internals.fsc2_clean_pid == 0 )
    {
        /* First of all wait for a single char from the parent, it doesn't
           matter which, we need just to know that the parent has run and
           thus knows about the childs PID. */

        close( pdt[ 1 ] );

        char c;
        while ( read( pdt[ 0 ], &c, 1 ) < 1 )
            /* empty */ ;
        close( pdt[ 0 ] );

        /* Make sure we start at the beginnig of the file - the last run
           may have been stopped by the user and the file may now be
           positioned somewhere in the middle */

        rewind( fp );
        lseek( fileno( fp ), 0, SEEK_SET );      /* paranoia... */

        /* Close read end of pipes output and errors messags are going to
           be written to */

        close( pd[ 0 ] );
        close( ed[ 0 ] );

        /* Redirct so that input is read from th file and writes to
           STOUT and STDERR go to the appropriate pipes */

        if (    dup2( fileno( fp ), STDIN_FILENO ) == -1
             || dup2( pd[ 1 ], STDOUT_FILENO ) == -1
             || dup2( ed[ 1 ], STDERR_FILENO ) == -1 )
        {
            ssize_t dummy;
            if ( errno == EMFILE )
                dummy = write( pd[ 1 ], "\x03\nStarting the test procedure "
                               "failed, running out of system resources.\n",
                               71 );
            else
                dummy = write( pd[ 1 ], "\x03\nStarting the test procedure "
                               "failed, internal error detected.\n", 63 );

            if ( dummy == -1 ) { /* silence stupid compiler warning */ }

            fclose( fp );
            close( pd[ 1 ] );
            close( ed[ 1 ] );

            goto filter_failure;
        }

        /* Finally. the redirected file descriptors must be closed */

        fclose( fp );
        close( pd[ 1 ] );
        close( ed[ 1 ] );

        if ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) )
        {
            execl( srcdir "fsc2_clean", "fsc2_clean", name, NULL );
            fputs( "\x03\nStarting the test procedure failed, utility '"
                   srcdir "fsc2_clean' probably not correctly installed.\n",
                   fp );
        }
        else
        {
            execl( bindir "fsc2_clean", "fsc2_clean", name, NULL );
            fputs( "\x03\nStarting the test procedure failed, utility '"
                   bindir "fsc2_clean' probably not correctly installed.\n",
                   fp );
        }

    filter_failure:

        fclose( stdout );
        _exit( EXIT_SUCCESS );
    }

    /* And, finally, the code for the parent: first thing to do is to send a
       single byte to the child so it knows we have its PID. */

    close( pdt[ 0 ] );
    char c = '0';
    ssize_t dummy = write( pdt[ 1 ], &c, 1 );
    close( pdt[ 1 ] );

    if ( dummy == -1 ) { /* silence stupid compiler warning */ }

    /* Close write side of the pipes, we're only going to read */

    close( pd[ 1 ] );
    close( ed[ 1 ] );

    /* Wait until the child process had a chance to write to the pipe. If the
       parent is too fast in trying to read on it it only may see an EOF. */

    fd_set rfds;
    FD_ZERO( &rfds );
    FD_SET( pd[ 0 ], &rfds );
    int rs;

    while (    ( rs = select( pd[ 0 ] + 1, &rfds, NULL, NULL, NULL ) ) == -1
            && errno == EINTR )
        /* empty */ ;

    if ( rs == -1 )
    {
        if ( errno == ENOMEM )
            print( FATAL, "Starting the test procedure failed, running out "
                   "of system resources.\n" );
#ifndef NDEBUG
        else
            fsc2_impossible( );
#endif
        close( pd[ 0 ] );
        close( ed[ 0 ] );
        return NULL;
    }

    *serr = ed[ 0 ];

    return fdopen( pd[ 0 ], "r" );
}


/*---------------------------------------------------------------------*
 * Replacement for usleep(), but you can decide by setting the second
 * argument if the function should continue to sleep on receipt of a
 * signal or if it should return immediately (in which case the return
 * value is -1 instead of 0). The first argument is the time to sleep
 * in microseconds, times longer then a second are allowed.
 *---------------------------------------------------------------------*/

int
fsc2_usleep( unsigned long us_dur,
             bool          quit_on_signal )
{
    struct timespec req;
    req.tv_sec = ( time_t ) us_dur / 1000000L;
    req.tv_nsec = ( us_dur % 1000000L ) * 1000;

    int ret;
    do
    {
        struct timespec rem;
        ret = nanosleep( &req, &rem );
        req = rem;
    } while ( ! quit_on_signal && ret == -1 && errno == EINTR );

    return ret;
}


/*--------------------------------------------------------------------------*
 * Functions checks if a supplied input string is identical to one of 'max'
 * alternatives, pointed to by 'alternatives', but neglecting the case of
 * the characters and removing leading and trailing white space from the
 * input string before the comparison. The comparison is case insensitive
 * and the function returns the index of the found alternative (a number
 * between 0 and max - 1) or -1 if none of the alternatives was identical
 * to the input string.
 *--------------------------------------------------------------------------*/

int
is_in( const char  * restrict supplied_in,
       const char ** restrict alternatives,
       int                    max )
{
    if ( ! supplied_in || ! *supplied_in || ! alternatives || ! max )
        return -1;

    /* Get pointer to the first non-white-space char in the user supplied
       input */

    while ( isspace( ( unsigned char ) *supplied_in ) && *++supplied_in )
        /* empty */ ;

    if ( ! *supplied_in )
        return -1;

    /* Figure out how many character there are in the user supplied input
       before it ends in all white-space characters */

    const char * end = supplied_in + strlen( supplied_in );
    while ( end > supplied_in && isspace( ( unsigned char ) end[ -1 ] ) )
        --end;

    if ( end == supplied_in )
        return -1;

    size_t len = end - supplied_in;

    /* Now check if the "valid" part of the user supplied input is identical
       (except for case) to one of the alternatives */

    const char * a = alternatives[ 0 ];
    for ( int cnt = 0; a && cnt < max; a = alternatives[ ++cnt ] )
        if ( ! strncasecmp( supplied_in, a, len ) )
            return cnt;

    return -1;
}


/*------------------------------------------------------------------------*
 * Function converts intensities into rgb values (between 0 and 255). For
 * values below 0 a dark kind of violet is returned, for values above 1 a
 * creamy shade of white. The interval [ 0, 1 ] itself is subdivided into
 * 6 subintervals at the points defined by the array 'p' and rgb colours
 * ranging from blue via cyan, green and yellow to red are calculated
 * with 'v' defining the intensities of the three primary colours at the
 * endpoints of the intervals and using linear interpolation in between.
 *------------------------------------------------------------------------*/

void
i2rgb( double   h,
       int    * rgb )
{
    static double p[ 7 ] = { 0.0, 0.125, 0.4, 0.5, 0.6, 0.875, 1.0 };
    static int v[ 3 ][ 7 ] = { {  64,   0,   0,  32, 233, 255, 191 },   // RED
                               {   0,  32, 233, 255, 233,  32,   0 },   // GREEN
                               { 191, 255, 233,  32,   0,   0,   0 } }; // BLUE

    if ( h < p[ 0 ] )           /* return very dark blue for values below 0 */
    {
        rgb[ RED   ] = 72;
        rgb[ GREEN ] =  0;
        rgb[ BLUE  ] = 72;
        return;
    }

    for ( int i = 0; i < 6; i++ )
    {
        if ( p[ i ] == p[ i + 1 ] || h > p[ i + 1 ] )
            continue;

        double scale = ( h - p[ i ] ) / ( p[ i + 1 ] - p[ i ] );
        for ( int j = RED; j <= BLUE; j++ )
            rgb[ j ] = irnd( v[ j ][ i ]
                             + ( v[ j ][ i + 1 ] - v[ j ][ i ] ) * scale );
        return;
    }

    rgb[ RED   ] = 255;  /* return a kind of creamy white for values above 1 */
    rgb[ GREEN ] = 248;
    rgb[ BLUE  ] = 220;
}


/*----------------------------------------------------------------*
 * Function creates a set of colours in XFORMs internal color map
 * for use in 2D graphics (NUM_COLORS is defined in global.h).
 *----------------------------------------------------------------*/

void
create_colors( void )
{
    /* Create the colours between blue and red */

    int rgb[ 3 ];

    for ( FL_COLOR i = 0; i < NUM_COLORS; i++ )
    {
        i2rgb( ( double ) i / ( double ) ( NUM_COLORS - 1 ), rgb );
        fl_mapcolor( i + FL_FREE_COL1,
                     rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );
    }

    /* Finally create colours for values too small or too large */

    i2rgb( -1.0, rgb );
    fl_mapcolor( NUM_COLORS + FL_FREE_COL1,
                 rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );

    i2rgb( 2.0, rgb );
    fl_mapcolor( NUM_COLORS + FL_FREE_COL1 + 1,
                 rgb[ RED ], rgb[ GREEN ], rgb[ BLUE ] );
}


/*-----------------------------------------------------*
 * Converts a string with a channel name into a number
 *-----------------------------------------------------*/

Var_T *
convert_to_channel_number( const char * channel_name )
{
    long channel;
    for ( channel = 0; channel < NUM_CHANNEL_NAMES; channel++ )
        if ( ! strcmp( channel_name, Channel_Names[ channel ] ) )
            break;

    /* If the name was not recognized the reason might by that the
       abbreviation "LIN" may have been used for "LINE" or instead of
       "TRIG_OUT" "TRIGGER_OUT" or "TRIGGEROUT" or "TRIGOUT"... */

    if ( channel == NUM_CHANNEL_NAMES )
    {
        if ( ! strcmp( channel_name, "LIN" ) )
            channel = CHANNEL_LINE;
        if (    ! strcmp( channel_name, "TRIGGER_OUT" )
             || ! strcmp( channel_name, "TRIGOUT" )
             || ! strcmp( channel_name, "TRIGGEROUT" ) )
            channel = CHANNEL_TRIG_OUT;
    }

    if ( channel == NUM_CHANNEL_NAMES )
        fsc2_impossible( );

    return vars_push( INT_VAR, channel );
}


/*-----------------------------------------------------------------------*
 * Function for determining the minimum of a function using the SIMPLEX
 * algorithm (see J.A. Nelder, R. Mead, Comp. J. 7, (1965), p. 308-313)
 * Input arguments:
 * 1. number of parameters of function
 * 2. array of starting values for the fit
 * 3. array of deviations of start values
 * 4. void pointer that gets passed on to the function to be minimized
 * 5. address of function to be minimized - function has to accept the
 *    following arguments:
 *    a. pointer to (double array) of parameters
 *    b. void pointer (can be used to transfer further required data)
 * 6. size of criterion for end of minimization: when the ratio between
 *    the standard error of the function values at the corners of the
 *    simplex to the mean of this function values is smaller than the
 *    size minimization is stopped.
 * When the function returns the data in the array with the start values
 * has been overwritten by the paramters that yield the mimimum value.
 *-----------------------------------------------------------------------*/

double
fsc2_simplex( size_t   n,
              double * x,
              double * dx,
              void *   par,
              double   ( *func )( double *, void * ),
              double   epsilon )
{
    double *p,              /* matrix of the (n + 1) simplex corners */
           *y,              /* array of function values at the corners */
           *p_centroid,     /* coordinates of center of simplex */
           *p_1st_try,      /* point after reflection */
           *p_2nd_try,      /* point after expansion/contraction */
           y_1st_try,       /* function value after reflection */
           y_2nd_try,       /* function value after expansion/contraction */
           y_lowest,        /* smallest function value */
           y_highest,       /* largest function value */
           y_2nd_highest,   /* second-largest function value */
           alpha = 1.0,     /* reflection factor*/
           beta = 0.5,      /* contraction factor*/
           gamm = 2.0;      /* expansion factor */
    size_t highest,         /* index of corner with largest/smallest */
           lowest;          /* function value */


    /* Get enough memory for the corners of the simplex, center points and
       function values at the (n + 1) corners */

    p = T_malloc( ( n * ( n + 5 ) + 1 ) * sizeof *p );
    p_centroid = p + n * ( n + 1 );
    p_1st_try = p_centroid + n;
    p_2nd_try = p_1st_try + n;
    y = p_2nd_try + n;

    /* Set up the corners of the simplex and calculate the function values
       at these positions */

    for ( size_t volatile i = 0; i < n + 1; i++ )
    {
        for ( size_t j = 0; j < n; j++ )
            p[ i * n  + j ] = x[ j ];

        if ( i != n )
            p[ i * ( n + 1 ) ] += dx[ i ];

        TRY
        {
            y[ i ] = func( p + i * n, par );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( p );
            RETHROW;
        }
    }

    /* Here starts the loop for finding the minimum. It will only stop when
       the minimum criterion is satisfied. */

    do
    {
        /* Determine index and function value of the corner with the lowest,
           the largest and the second largest function value */

        lowest = ( y[ 0 ] < y[ 1 ] ) ? ( highest = 1, 0 ) : ( highest = 0, 1 );

        y_lowest = y_2nd_highest = y[ lowest ];
        y_highest = y[ highest ];

        for ( size_t i = 2; i < n + 1; i++ )
        {
            if ( y[ i ] < y_lowest )
                y_lowest = y[ lowest = i ];

            if ( y[ i ] > y_highest )
            {
                y_2nd_highest = y_highest;
                y_highest = y[ highest = i ];
            }
            else
                if ( y[ i ] > y_2nd_highest )
                    y_2nd_highest = y[ i ];
        }

        /* Calculate center of simplex (excluding the corner with the largest
           function value) */

        for ( size_t j = 0; j < n; j++ )
        {
            p_centroid[ j ] = 0.0;
            for ( size_t i = 0; i < n + 1; i++ )
            {
                if ( i == highest )
                    continue;
                p_centroid[ j ] += p[ i * n + j ];
            }

            p_centroid[ j ] /=  n;
        }

        /* Do a reflection, i.e. mirror the point with the largest function
           value at the center point and calculate the function value at
           this new point */

        for ( size_t j = 0; j < n; j++ )
            p_1st_try[ j ] = ( 1.0 + alpha ) * p_centroid[ j ]
                             - alpha * p[ highest * n + j ];

        y_1st_try = func( p_1st_try, par );

        /* If the function value at the new point is smaller than the largest
           value, the new point replaces the corner with the largest function
           value */

        if ( y_1st_try < y_highest )
        {
            for ( size_t j = 0; j < n; j++ )
                p[ highest * n + j ] = p_1st_try[ j ];

            y[ highest ] = y_1st_try;
        }

        /* If the function value at the new point even smaller than all other
           function values try an expansion of the simplex */

        if ( y_1st_try < y_lowest )
        {
            for ( size_t j = 0; j < n; j++ )
                p_2nd_try[ j ] = gamm * p_1st_try[ j ] +
                                 ( 1.0 - gamm ) * p_centroid[ j ];

            y_2nd_try = func( p_2nd_try, par );

            /* If the expansion was successful, i.e. led to an even smaller
               function value, replace the corner with the largest function
               by this new improved new point */

            if ( y_2nd_try < y_1st_try )
            {
                for ( size_t j = 0; j < n; j++ )
                    p[ highest * n + j ] = p_2nd_try[ j ];

                y[ highest ] = y_2nd_try;
            }

            continue;                     /* start next cycle */
        }

        /* If the new point is at least smaller than the previous maximum
           value start a new cycle, otherwise try a contraction */

        if ( y_1st_try < y_2nd_highest )
            continue;                     /* start next cycle */

        for ( size_t j = 0; j < n; j++ )
            p_2nd_try[ j ] = beta * p[ highest * n + j ]
                             + ( 1.0 - beta ) * p_centroid[ j ];

        y_2nd_try = func( p_2nd_try, par );

        /* If the contraction was successful, i.e. led to a value that is
           smaller than the previous maximum value, the new point replaces
           the largest value. Otherwise the whole simplex gets contracted
           by shrinking all edges by a factor of 2 toward the point toward
           the point with the smallest function value. */

        if ( y_2nd_try < y[ highest ] )
        {
            for ( size_t j = 0; j < n; j++ )
                p[ highest * n +  j ] = p_2nd_try[ j ];

            y[ highest ] = y_2nd_try;
        }
        else
            for ( size_t i = 0; i < n + 1; i++ )
            {
                if ( i == lowest )
                    continue;

                for ( size_t j = 0; j < n; j++ )
                    p[ i * n + j ] =
                                0.5 * ( p[ i * n + j ] + p[ lowest * n +  j ]);

                y[ i ] = func( p + n * i, par );
            }
    } while ( ! fsc2_simplex_is_minimum( n + 1, y, epsilon ) );

    /* Pick the corner with the lowest function value and write its
       coordinates over the array with the start values, deallocate
       memory and return the minimum function value */

    y_lowest = y[ 0 ];

    for ( size_t i = 1; i < n + 1; i++ )
        if ( y_lowest > y[ i ] )
            y_lowest = y[ lowest = i ];

    for ( size_t j = 0; j < n; j++ )
        x[ j ] = p[ lowest * n + j ];

    T_free( p );

    return y_lowest;
}


/*-------------------------------------------------------------------*
 * Tests for the simplex() function if the minimum has been reached.
 * It checks if the ratio of the standard error of the function
 * values at the corners of the simplex and the mean of the function
 * values is smaller than a constant 'epsilon'.
 * Arguments:
 * 1. number of corners of the simplex
 * 2. array of function values at the corners
 * 3. constant 'epsilon'
 * Function returns 1 when the minimum has been found, 0 otherwise.
 *-------------------------------------------------------------------*/

static int
fsc2_simplex_is_minimum( int      n,
                         double * y,
                         double   epsilon )
{
    double yq   = 0.0,          /* sum of function values */
           yq_2 = 0.0;          /* Sum of squares of function values */

    for ( int i = 0; i < n; i++ )
    {
        yq += y[ i ];
        yq_2 += y[ i ] * y[ i ];
    }

    /* standard error of fucntion values */

    double dev = sqrt( ( yq_2 - yq * yq / n ) /  ( n - 1 ) );

    return dev * n / fabs( yq ) < epsilon;
}


/*----------------------------------------------------------------*
 * Reads a line of text of max_len characters ending in '\n' from
 * a socket. This is directly copied from W. R. Stevens, UNP1.2.
 *----------------------------------------------------------------*/

ssize_t
read_line( int    fd,
           void * vptr,
           size_t max_len )
{
    ssize_t n;
    char *ptr = vptr;

    for ( n = 1; n < ( ssize_t ) max_len; n++ )
    {
        char c;
        ssize_t rc;
        if ( ( rc = do_read( fd, &c ) ) == 1 )
        {
            *ptr++ = c;
            if ( c == '\n' )
                break;
        }
        else if ( rc == 0 )
        {
            if ( n == 1 )
                return 0;
            else
                break;
        }
        else
            return -1;
    }

    *ptr = '\0';
    return n;
}


/*-----------------------------------------------------*
 * This is directly copied from W. R. Stevens, UNP1.2.
 *-----------------------------------------------------*/

static ssize_t
do_read( int    fd,
         char * ptr )
{
    static int read_cnt;
    static char *read_ptr;
    static char read_buf[ MAX_LINE_LENGTH ];

    if ( read_cnt <= 0 )
    {
      again:
        if ( ( read_cnt = read( fd, read_buf, sizeof read_buf ) ) < 0 )
        {
            if ( errno == EINTR )
                goto again;
            return -1;
        }
        else if ( read_cnt == 0 )
            return 0;

        read_ptr = read_buf;
    }

    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}


/*-----------------------------------------------------*
 * Write a line of a n characters of text to a socket.
 * This is directly copied from W. R. Stevens, UNP1.2.
 *-----------------------------------------------------*/

ssize_t
writen( int          fd,
        const void * vptr,
        size_t       n )
{
    const char *ptr = vptr;
    size_t nleft = n;

    while ( nleft > 0 )
    {
        ssize_t nwritten;

        if ( ( nwritten = write( fd, ptr, nleft ) ) <= 0 )
        {
            if ( errno == EINTR )
                nwritten = 0;
            else
                return -1;
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

    return n;
}


/*-----------------------------------------------------------------------*
 * This function is just a wrapper around fl_show_fselector() to be able
 * to write out the new directory setting afterwards.
 *-----------------------------------------------------------------------*/

const char *
fsc2_show_fselector( const char * message,
                     const char * dir,
                     const char * pattern,
                     const char * def_name )
{
    if ( dir && ! *dir )
        dir = NULL;

    const char * ret = fl_show_fselector( message, dir, pattern, def_name );

    /* Save the possibly new setting to the configuration file */

    fsc2_save_conf( );

    return ret;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

#define FGETS_START_LEN 128

char *
fsc2_fline( FILE * fp )
{


    if ( ! fp )
        THROW( EXCEPTION );

    char * volatile line = T_malloc( FGETS_START_LEN );
    char * volatile p = line;
    *line = '\0';

    size_t volatile  buf_len = FGETS_START_LEN;
    size_t volatile rem_len = buf_len;

    while ( true )
    {
        if ( ! fgets( p, rem_len, fp ) || ! *p )
            break;

        size_t len = strlen( p );

        if ( p[ len - 1 ] == '\n' && p[ len - 2 ] != '\\' )
            break;

        if ( p[ len - 2 ] == '\\' )
            len -= 2;

        rem_len -= len;
        p += len;

        size_t offset = p - line;

        TRY
        {
            line = T_realloc( line, 2 * buf_len );
            p = line + offset;
            rem_len += buf_len;
            buf_len *= 2;
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            T_free( line );
            RETHROW;
        }
    }

    if ( ! *line )
    {
        T_free( line );
        return NULL;
    }

    TRY
    {
        line = realloc( line, strlen( line ) + 1 );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        T_free( line );
        RETHROW;
    }

    return line;
}


/*--------------------------------------------------------------------*
 *--------------------------------------------------------------------*/

static int
get_decoration_sizes( FL_FORM * form,
                      int     * top,
                      int     * right,
                      int     * bottom,
                      int     * left )
{
	if (    ! form
		 || ! form->window
		 || form->visible != FL_VISIBLE
		 || form->parent )
		return 1;

	*top = *right = *bottom = *left = 0;

	Atom a = XInternAtom( fl_get_display( ), "_NET_FRAME_EXTENTS", True );

    if ( a != None )
	{
		Atom actual_type;
		int actual_format;
		unsigned long nitems;
		unsigned long bytes_after;
		long * prop = NULL;

        XGetWindowProperty( fl_get_display( ), form->window, a, 0,
                            4, False, XA_CARDINAL,
                            &actual_type, &actual_format, &nitems,
                            &bytes_after, ( unsigned char ** ) &prop );

		if (    actual_type == XA_CARDINAL
			 && actual_format == 32
			 && nitems == 4 )
		{
			*top    = prop[ 2 ];
			*right  = prop[ 1 ];
			*bottom = prop[ 3 ];
			*left   = prop[ 0 ];
		}

        XFree( prop );
	}
	else
	{
		/* The window manager doesn't have the _NET_FRAME_EXTENDS atom so we
		   have to try with the traditional method (which assumes that the
           window manager reparents the windows it manages) */

		Window cur_win = form->window;
		Window root;
		Window parent;
		Window *childs = NULL;
		Window wjunk;
		XWindowAttributes win_attr;
		XWindowAttributes frame_attr;
		unsigned int ujunk;

        /* Get the coordinates and size of the form's window */

		XGetWindowAttributes( fl_get_display( ), cur_win, &win_attr );

        /* Check try to get its parent window */

		XQueryTree( fl_get_display( ), cur_win, &root, &parent, &childs,
                    &ujunk );
		if ( childs )
		{
			XFree( childs );
			childs = NULL;
		}

		/* If there's no parent or the parent window is the root window
		   we've got to assume that there are no decorations */

		if ( ! parent || parent == root )
			return 0;

        /* Now convert the form window's coordiates to that relative to the
		   root window and the find the top-most parent (that isn't the root
		   window itself) */

		XTranslateCoordinates( fl_get_display( ), parent, root,
							   win_attr.x, win_attr.y,
							   &win_attr.x, &win_attr.y, &wjunk );

		while ( parent && parent != root )
		{
			cur_win = parent;
			XQueryTree( fl_get_display( ), cur_win, &root, &parent, &childs,
						&ujunk );
			if ( childs )
			{
				XFree( childs );
				childs = NULL;
			}
		}

        /* Get the cordinates and sizes of that top-most window... */

		XGetWindowAttributes( fl_get_display( ), cur_win, &frame_attr );

        /* ...and finally calculate the decoration sizes */

		*top    = win_attr.y - frame_attr.y;
		*left   = win_attr.x - frame_attr.x;
		*bottom = frame_attr.height - win_attr.height - *top;
		*right  = frame_attr.width - win_attr.width   - *left;
	}

	return 0;
}


/*--------------------------------------------------------------------*
 * Given a form the function tries to determine the exact position of
 * its window, including window manager decorations. The resulting
 * values should be usable directly for a geometry string to be
 * passed as position of the window to the XForms library.
 *--------------------------------------------------------------------*/

void
get_form_position( FL_FORM * form,
                   int     * x,
                   int     * y )
{
    int w,
        h;
    int top,
        right,
        bottom,
        left;

    fl_get_wingeometry( form->window, x, y, &w, &h );
    get_decoration_sizes( form, &top, &right, &bottom, &left );

    /* If the windows left upper corner is not on the scrreen but left to
       to left screen border or above the tob border the corresponding
       value is negative. But such a value would, when used for setting
       a window position, be interpreted as relative to the right or botton
       screen border, os correct them so that they can be interpreted that
       way */

    *x -= left;
    if ( *x < 0 )
        *x += left + w + right - fl_scrw;

    *y -= top;
    if ( *y < 0 )
        *y += top + h + bottom - fl_scrh;
}


/*-------------------------------------*
 * Save some configuration information
 *-------------------------------------*/

void
fsc2_save_conf( void )
{
    if ( Fsc2_Internals.cmdline_flags & NO_GUI_RUN )
        return;

    struct passwd *ue = getpwuid( getuid( ) );
    if ( ! ue || ! ue->pw_dir || ! *ue->pw_dir )
         return;

    char * fname;
    TRY
    {
        fname = get_string( "%s/.fsc2/fsc2_config", ue->pw_dir );
        TRY_SUCCESS;
    }
    OTHERWISE
        return;

    FILE * fp = fopen( fname, "w" );
    if ( ! fp || ! fsc2_obtain_fcntl_lock( fp, F_WRLCK, true ) )
    {
        if ( fp )
            fclose( fp );
        T_free( fname );
        return;
    }

    T_free( fname );

    fprintf( fp, "# Don't edit this file - it gets overwritten "
             "automatically at irregular times\n\n" );

    fprintf( fp, "DEFAULT_DIRECTORY: \"%s\"\n", fl_get_directory( ) );

    get_form_position( GUI.main_form->fsc2, &GUI.win_x, &GUI.win_y );
    GUI.win_width  = GUI.main_form->fsc2->w;
    GUI.win_height = GUI.main_form->fsc2->h;

    fprintf( fp, "MAIN_WINDOW_POSITION: %+d%+d\nMAIN_WINDOW_SIZE: %ux%u\n",
             GUI.win_x, GUI.win_y, GUI.win_width, GUI.win_height );

    if ( GUI.display_1d_has_pos )
        fprintf( fp, "DISPLAY_1D_WINDOW_POSITION: %+d%+d\n", GUI.display_1d_x,
                 GUI.display_1d_y );

    if ( GUI.display_1d_has_size )
        fprintf( fp, "DISPLAY_1D_WINDOW_SIZE: %ux%u\n", GUI.display_1d_width,
                 GUI.display_1d_height );

    if ( GUI.display_2d_has_pos )
        fprintf( fp, "DISPLAY_2D_WINDOW_POSITION: %+d%+d\n", GUI.display_2d_x,
                 GUI.display_2d_y );

    if ( GUI.display_2d_has_size )
        fprintf( fp, "DISPLAY_2D_WINDOW_SIZE: %ux%u\n", GUI.display_2d_width,
                 GUI.display_2d_height );

    if ( GUI.cut_win_has_pos )
        fprintf( fp, "CUT_WINDOW_POSITION: %+d%+d\n", GUI.cut_win_x,
                 GUI.cut_win_y );

    if ( GUI.cut_win_has_size )
        fprintf( fp,"CUT_WINDOW_SIZE: %ux%u\n", GUI.cut_win_width,
                 GUI.cut_win_height );

    if ( GUI.toolbox_has_pos )
        fprintf( fp, "TOOLBOX_POSITION: %+d%+d\n", GUI.toolbox_x,
                 GUI.toolbox_y );

    fsc2_release_fcntl_lock( fp );
    fclose( fp );
}


/*--------------------------------------------------------------*
 * Function tries to find the maximum path length on the system
 *--------------------------------------------------------------*/

#define PATH_MAX_GUESS 4095      /* guess for maximum file name length... */

size_t
get_pathmax( void )
{
#if defined PATH_MAX
    static long pathmax = PATH_MAX;
#else
    static long pathmax = 0;
#endif

    if ( pathmax == 0 )
    {
        if ( ( pathmax = pathconf( "/", _PC_PATH_MAX ) ) < 0 )
        {
            if ( errno == 0 )
                pathmax = PATH_MAX_GUESS;
            else
            {
                eprint( FATAL, false, "Failure to determine the maximum "
                        "path length on this system.\n" );
                THROW( EXCEPTION );
            }
        }
    }

    return pathmax;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
