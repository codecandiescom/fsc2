/* -*-C-*-
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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

%option noyywrap stack noyy_top_state noinput nounput never-interactive

%{

#include "hjs_sfc.h"


static long hjs_sfc_Lc,
            hjs_sfc_Comm_Lc;


/* The following declarations are to avoid unnecessary warnings from
   the flex-generated C code for some versions of flex */

#if defined FLEX_NEEDS_DECLARATIONS
int hjs_sfc_get_lineno( void );
FILE * hjs_sfc_get_in( void );
FILE * hjs_sfc_get_out( void );
int hjs_sfc_get_leng( void );
char * hjs_sfc_get_text( void );
void hjs_sfc_set_lineno( int );
void hjs_sfc_set_in( FILE * );
void hjs_sfc_set_out( FILE * );
int hjs_sfc_get_debug( void );
void hjs_sfc_set_debug( int );
int hjs_sfc_lex_destroy( void );
#endif

%}

REM1     ^[\t ]*("//"|"#").*\n
REM2     [\t ]*("//"|"#").*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n
WLWS     ^[\t ,;:]*\n
WS       [\t ,;:]+
TWS      [\t ,;:]+\n

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    [+-]?((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})
D        ([+-]?{INT})|{FLOAT}
UNIT     "G"

%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      hjs_sfc_Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      hjs_sfc_Lc++;

            /* handling of C style comment */
{REM3}      {
                hjs_sfc_Comm_Lc = hjs_sfc_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          hjs_sfc_Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     {
                print( FATAL, "End of calibration file '%s' detected within a "
                       "comment.\n", hjs_sfc.calib_file );
                fclose( hjs_sfc_in );
                THROW( EXCEPTION );
            }

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                hjs_sfc_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     {
                print( FATAL, "End of comment indicator (\"*/\") without "
                       "previous start of comment in calibration file '%s' "
                       "at line %ld.\n", hjs_sfc.calib_file, hjs_sfc_Lc );
                fclose( hjs_sfc_in );
                THROW( EXCEPTION );
            }

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      hjs_sfc_Lc++;

            /* dumps trailing white space */
{TWS}       hjs_sfc_Lc++;

            /* dump all other forms of white space as well as units */
{WS}
{UNIT}

            /* count lines */
\n          hjs_sfc_Lc++;

{D}         return 1;

<<EOF>>     return 0;

.           {
                print( FATAL, "Invalid input in calibration file '%s' at "
                       "line %ld.\n", hjs_sfc.calib_file, hjs_sfc_Lc );
                fclose( hjs_sfc_in );
                THROW( EXCEPTION );
            }

        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void
hjs_sfc_read_calibration( void )
{
    static bool is_restart = UNSET;
    int num_entries;


    /* Try to open the configuration file */

    if ( hjs_sfc.calib_file == NULL )
    {
        if ( DEFAULT_CALIB_FILE[ 0 ] ==  '/' )
            hjs_sfc.calib_file = T_strdup( DEFAULT_CALIB_FILE );
        else
            hjs_sfc.calib_file = get_string( "%s%s%s", libdir, slash( libdir ),
                                             DEFAULT_CALIB_FILE );
    }

    if ( access( hjs_sfc.calib_file, R_OK ) == -1 )
    {
        if ( errno == ENOENT )       /* file not found */
            print( FATAL, "Calibration file '%s' not found.\n",
                   hjs_sfc.calib_file );
        else
            print( FATAL, "No read permission for calibration file '%s'.\n",
                   hjs_sfc.calib_file );
        THROW( EXCEPTION );
    }

    if ( ( hjs_sfc_in = fopen( hjs_sfc.calib_file, "r" ) ) == NULL )
    {
        print( FATAL, "Can't open calibration file '%s'.\n",
               hjs_sfc.calib_file );
        THROW( EXCEPTION );
    }

    /* Keep the lexer happy... */

    if ( is_restart )
        hjs_sfc_restart( hjs_sfc_in );
    else
        is_restart = SET;

    hjs_sfc_Lc = 1;

    for ( num_entries = 0; hjs_sfc_lex( ) != 0; num_entries++ )
        switch ( num_entries )
        {
            case 0 :
                hjs_sfc.B0V = T_atod( hjs_sfc_text );
                break;

            case 1 :
                hjs_sfc.slope = 0.1 * ( T_atod( hjs_sfc_text ) - hjs_sfc.B0V );
                break;

            default :
                print( FATAL, "Too many entries in configuration file '%s'.\n",
                       hjs_sfc.calib_file );
                fclose( hjs_sfc_in );
                THROW( EXCEPTION );
        }

    fclose( hjs_sfc_in );

    /* Check that there were exactly two entries int the calibration file */

    if ( num_entries < 2 )
    {
        print( FATAL, "Calibration file '%s' contains %s, but both the field "
               "at the minimum and maximum DAC output voltage is required.\n",
               hjs_sfc.calib_file, num_entries ? "only one entry" :
               "no entries" );
        THROW( EXCEPTION );
    }

    /* Check that the entries weren't completely bogus */

    if ( hjs_sfc.B0V < 0.0 )
    {
        print( FATAL, "Invalid negative value for field at DAC output "
               "of 0 V in calibration file '%s'.\n", hjs_sfc.calib_file );
        THROW( EXCEPTION );
    }

    if ( hjs_sfc.slope == 0.0 )
    {
        print( FATAL, "According to calibration file '%s' the field does not "
               "change with DAC output voltage.\n", hjs_sfc.calib_file );
        THROW( EXCEPTION );
    }
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
