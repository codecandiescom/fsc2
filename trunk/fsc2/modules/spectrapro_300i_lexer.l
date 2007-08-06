/* -*-C-*-
 *  $Id$
 * 
 *  Copyright (C) 1999-2007 Jens Thoms Toerring
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


        /*---------------------*/
        /*     DEFINITIONS     */
        /*---------------------*/

%option noyywrap stack noyy_top_state nounput never-interactive

%{

#include "spectrapro_300i.h"


static void spectrapro_300i_new_entry( int          type,
                                       const char * calib_file );

static long spectrapro_300i_Lc,
            spectrapro_300i_Comm_Lc;

static struct {
    double val[ 3 ];
    bool is_calib[ 3 ];
} spectrapro_300i_calibs[ MAX_GRATINGS ];


#define IA_ENTRY    1
#define FL_ENTRY    2
#define DA_ENTRY    3
#define DATA_TYPE   4
#define DATA_U_TYPE 5


/* We need these to avoid unnecessary warnings from the flex-generated
   C code when newer versions of flex are used */

int spectrapro_300i_get_lineno( void );
FILE * spectrapro_300i_get_in( void );
FILE * spectrapro_300i_get_out( void );
int spectrapro_300i_get_leng( void );
char * spectrapro_300i_get_text( void );
void spectrapro_300i_set_lineno( int );
void spectrapro_300i_set_in( FILE * );
void spectrapro_300i_set_out( FILE * );
int spectrapro_300i_get_debug( void );
void spectrapro_300i_set_debug( int );
int spectrapro_300i_lex_destroy( void );

%}

REM1     ^[\t ]*("//"|"#").*\n
REM2     [\t ]*("//"|"#").*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n
WLWS     ^[\t ,;:]*\n
WS       [\t ,;:=]+
TWS      [\t ,;:]+\n

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    [+-]?((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})
D        ([+-]?{INT})|{FLOAT}
IAE      INCLUSION_ANGLE_?{INT}
FLE      FOCAL_LENGTH_?{INT}
DAE      DETECTOR_ANGLE_?{INT}
DU       (([+-]?{INT})|{FLOAT})[\t ]?("M"|"k"|"m"|"u"|"n")?"m"

%x      comm


        /*---------------*/
%%      /*     RULES     */
        /*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}      spectrapro_300i_Lc++;

            /* handling of C++ style comment not spanning a whole line */
{REM2}      spectrapro_300i_Lc++;

            /* handling of C style comment */
{REM3}      {
                spectrapro_300i_Comm_Lc = spectrapro_300i_Lc;
                yy_push_state( comm );
            }

<comm>{

{REM4}      /* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

            /* end of line character in a comment */
\n          spectrapro_300i_Lc++;

            /* handling of EOF within a comment -> fatal error */
<<EOF>>     THROW( EOF_IN_COMMENT_EXCEPTION );

            /* end of comment but not end of line */
{EREM1}     yy_pop_state( );

            /* end of comment and end of line */
{EREM2}     {
                spectrapro_300i_Lc++;
                yy_pop_state( );
            }
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT );

{IAE}       return IA_ENTRY;
{FLE}       return FL_ENTRY;
{DAE}       return DA_ENTRY;

{DU}        return DATA_U_TYPE;
{D}         return DATA_TYPE;

            /* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      spectrapro_300i_Lc++;

            /* dumps trailing white space */
{TWS}       spectrapro_300i_Lc++;

            /* dump all other forms of white */
{WS}

            /* count lines */
\n          spectrapro_300i_Lc++;

<<EOF>>     return 0;

.           THROW( INVALID_INPUT_EXCEPTION );


        /*----------------------*/
%%      /*     END OF RULES     */
        /*----------------------*/


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

void spectrapro_300i_read_calib( FILE *       fp,
                                 const char * calib_file )
{
    static bool is_restart = UNSET;
    int type;
    int i;
    int j;
    int num_calibs;


    if ( ( spectrapro_300i_in = fp ) == NULL )
    {
        print( FATAL, "Internal error while reading calibration file.\n" );
        THROW( EXCEPTION );
    }

    for ( i = 0; i < MAX_GRATINGS; i++ )
        for ( j = 0; j < 3; j++ )
            spectrapro_300i_calibs[ i ].is_calib[ j ] = UNSET;

    /* Keep the lexer happy... */

    if ( is_restart )
        spectrapro_300i_restart( spectrapro_300i_in );
    else
        is_restart = SET;

    spectrapro_300i_Lc = 1;

    TRY 
    {
        while( ( type = spectrapro_300i_lex( ) ) != 0 )
            spectrapro_300i_new_entry( type, calib_file );
        TRY_SUCCESS;
    }
    CATCH( EOF_IN_COMMENT_EXCEPTION )
    {
        print( FATAL, "Unexpected end of table file '%s' in comment starting "
               "at line %ld.\n", calib_file, spectrapro_300i_Comm_Lc );
        THROW( EXCEPTION );
    }
    CATCH( DANGLING_END_OF_COMMENT )
    {
        print( FATAL, "End of comment found in table file '%s' at line %ld.\n",
               calib_file, spectrapro_300i_Lc );
        THROW( EXCEPTION );
    }
    CATCH( INVALID_INPUT_EXCEPTION )
    {
        print( FATAL, "Invalid entry in table file '%s' at line %ld.\n",
               calib_file, spectrapro_300i_Lc );
        THROW( EXCEPTION );
    }
    OTHERWISE
        RETHROW( );

    for ( i = 0; i < MAX_GRATINGS; i++ )
    {
        num_calibs = 0;

        if ( spectrapro_300i_calibs[ i ].is_calib[ 0 ] )
        {
            spectrapro_300i.grating[ i ].inclusion_angle =
                                          spectrapro_300i_calibs[ i ].val[ 0 ];
            num_calibs++;
        }

        if ( spectrapro_300i_calibs[ i ].is_calib[ 1 ] )
        {
            spectrapro_300i.grating[ i ].focal_length =
                                          spectrapro_300i_calibs[ i ].val[ 1 ];
            num_calibs++;
        }

        if ( spectrapro_300i_calibs[ i ].is_calib[ 2 ] )
        {
            spectrapro_300i.grating[ i ].detector_angle =
                                          spectrapro_300i_calibs[ i ].val[ 2 ];
            num_calibs++;
        }

        spectrapro_300i.grating[ i ].is_calib = num_calibs == 3;
    }
}


/*-------------------------------------------------------------*
 *-------------------------------------------------------------*/

static void spectrapro_300i_new_entry( int          type,
                                       const char * calib_file )
{
    char *cp;
    long grating;
    double value;
    int value_type;
    const char *type_names[ ] = { "inclusion angle", "focal length",
                                  "detector angle" };


    if ( type < IA_ENTRY && type > DA_ENTRY )
    {
        print( FATAL, "Invalid entry in table file '%s' at line %ld.\n",
               calib_file, spectrapro_300i_Lc );
        THROW( EXCEPTION );
    }

    type--;

    for ( cp = spectrapro_300i_text + spectrapro_300i_leng - 1;
          cp > spectrapro_300i_text && isdigit( ( unsigned char ) *cp ); cp-- )
        /* empty */;

    if ( cp == spectrapro_300i_text )
    {
        print( FATAL, "Invalid entry in calibration file '%s' at line %ld.\n",
               calib_file, spectrapro_300i_Lc );
        THROW( EXCEPTION );
    }

    grating = T_atol( ++cp ) - 1;

    if ( grating < 0 || grating >= MAX_GRATINGS )
    {
        print( FATAL, "Invalid entry (grating number out of range) in "
               "calibration file '%s' at line %ld.\n", calib_file,
               spectrapro_300i_Lc );
        THROW( EXCEPTION );
    }

    if ( spectrapro_300i_calibs[ grating ].is_calib[ type ] )
        print( SEVERE, "Repeated definition of %s for grating %ld at line "
               "%ld of calibration file '%s'.\n", type_names[ type ],
               grating + 1, spectrapro_300i_Lc, calib_file );

    value_type = spectrapro_300i_lex( );

    if (    ( value_type != DATA_TYPE && value_type != DATA_U_TYPE )
         || ( type != FL_ENTRY - 1 && value_type == DATA_U_TYPE ) )
    {
        print( FATAL, "Invalid entry in calibration file '%s' at line %ld.\n",
               calib_file, spectrapro_300i_Lc );
        THROW( EXCEPTION );
    }

    value = T_atod( spectrapro_300i_text );

    if ( value_type == DATA_U_TYPE )
    {
        for ( cp = spectrapro_300i_text + spectrapro_300i_leng - 1;
              cp > spectrapro_300i_text && isalpha( ( unsigned char ) *cp );
              cp-- )
            /* empty */;

        if ( cp++ == spectrapro_300i_text )
        {
            print( FATAL, "Invalid entry in calibration file '%s' at "
                   "line %ld.\n", calib_file, spectrapro_300i_Lc );
            THROW( EXCEPTION );
        }

        switch ( *cp )
        {
            case 'M' :
                value *= 1.0e6;
                break;

            case 'k' :
                value *= 1.0e3;
                break;

            case 'm' :
                if ( *( cp + 1 ) == 'm' )
                    value *= 1.0e-3;
                break;

            case 'u' :
                value *= 1.0e-6;
                break;

            case 'n' :
                value *= 1.0e-9;
                break;

            default :
                fsc2_impossible( );
        }
    }

    /* Negative values for the inclusion angle or the focal length can't
       be right... */

    if ( ( type + 1 == IA_ENTRY || type + 1 == FL_ENTRY ) && value <= 0.0 )
    {
        print( FATAL, "Invalid negative value for %s at line %ld in "
               "calibration file '%s'.\n", type_names[ type ],
               spectrapro_300i_Lc, calib_file );
        THROW( EXCEPTION );
    }

    if ( type + 1 == FL_ENTRY )
        spectrapro_300i_calibs[ grating ].val[ type ] = value;
    else
        spectrapro_300i_calibs[ grating ].val[ type ] =
                                                      value * atan( 1.0 ) / 45;
    spectrapro_300i_calibs[ grating ].is_calib[ type ] = SET;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
