/* -*-C-*-
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
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


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap stack noinput nounput noyy_top_state never-interactive

%{

#include "fsc2.h"

static long Comm_lc;

/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int devices_listget_lineno( void );
FILE * devices_listget_in( void );
FILE * devices_listget_out( void );
int devices_listget_leng( void );
char * devices_listget_text( void );
void devices_listset_lineno( int );
void devices_listset_in( FILE * );
void devices_listset_out( FILE * );
int devices_listget_debug( void );
void devices_listset_debug( int );
int devices_listlex_destroy( void );
#endif


%}

REM1     ^[\t ]*"//".*\n
REM2     "//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

IDENT    [A-Za-z0-9][A-Za-z_0-9]*

%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      EDL.Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      EDL.Lc++;

            /* handling of C style comment */
{REM3}      {
                Comm_lc = EDL.Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          EDL.Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     {
                eprint( FATAL, UNSET, "%s: End of device data base '%s' in "
                               "comment starting at line %ld.\n",
                               EDL.Fname, Comm_lc );
                THROW( EXCEPTION );
            }

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                EDL.Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     {  /* End of comment without start */
                eprint( FATAL, UNSET, "End of comment found at line %ld in "
                               "device data base '%s'.\n", EDL.Lc, EDL.Fname );
                THROW( EXCEPTION );
            }

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      EDL.Lc++;

{LWS}       /* dumps of leading white space */

            /* dumps trailing white space */
{TWS}       EDL.Lc++;

{WS}        /* dum (other) white space */

            /* writes out EOL character */
\n          EDL.Lc++;

,           /* skip commas */
;           /* skip semicolon */

{IDENT}     return 1;

.           {
                eprint( FATAL, UNSET, "Syntax error in devices data base '%s' "
                               "at line %ld.\n", EDL.Fname, EDL.Lc );
                THROW( EXCEPTION );
            }

<<EOF>>     return 0;



        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------*
 *-------------------------------------------*/

bool
device_list_parse( void )
{
    static bool is_restart = UNSET;
    Device_Name_T *new_device_name;
    struct stat buf;
    char *ld_path;
    char *ld = NULL;
    char *ldc;


    EDL.Fname = T_free( EDL.Fname );

    /* If we're supposed only to use the files from the source directories
       only try to use those */

    if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
    {
        EDL.Fname = get_string( confdir "Devices" );
        if ( lstat( EDL.Fname, &buf ) < 0 )
            EDL.Fname = T_free( EDL.Fname );
    }
    else
    {
        /* As usual, we first check paths defined by the environment variable
           'LD_LIBRARY_PATH' and then the compiled-in path (except when this
           is a check run). */

        if ( ( ld_path = getenv( "LD_LIBRARY_PATH" ) ) != NULL )
        {
            ld = T_strdup( ld_path );

            for ( ldc = strtok( ld, ":" ); ldc != NULL;
                  ldc = strtok( NULL, ":" ) )
            {
                EDL.Fname = get_string( "%s%sDevices", ldc, slash( ldc ) );
                if ( lstat( EDL.Fname, &buf ) == 0 )
                    break;
                EDL.Fname = T_free( EDL.Fname );
            }

            T_free( ld );
        }

        if (    EDL.Fname == NULL
             && ! ( Fsc2_Internals.cmdline_flags & DO_CHECK ) )
        {
            EDL.Fname = T_strdup( libdir "Devices" );
            if ( lstat( EDL.Fname, &buf ) < 0 )
                EDL.Fname = T_free( EDL.Fname );
        }
    }

    if ( EDL.Fname == NULL )
    {
        eprint( FATAL, UNSET, "Can't open device data base 'Devices'.\n" );
        return FAIL;
    }

    if ( ( devices_listin = fopen( EDL.Fname, "r" ) ) == NULL )
    {
        eprint( FATAL, UNSET, "Can't open device data base '%s'.\n",
                       EDL.Fname );
        EDL.Fname = T_free( EDL.Fname );
        return FAIL;
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        devices_listrestart( devices_listin );
    else
         is_restart = SET;

    EDL.Lc = 1;

    TRY
    {
        while ( devices_listlex( ) )
        {
            new_device_name = T_malloc( sizeof *new_device_name );
            new_device_name->name = T_strdup( devices_listtext );
            string_to_lower( new_device_name->name );
            new_device_name->next = EDL.Device_Name_List;
            EDL.Device_Name_List = new_device_name;
        }
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fclose( devices_listin );
        RETHROW( );
    }

    fclose( devices_listin );

    return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
