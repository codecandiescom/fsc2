/*
  $Id$

  Copyright (C) 2001 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


/**********************************************/
/* This is the main lexer for EDL input files */
/**********************************************/


%option noyywrap case-sensitive nounput

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                  \
{                                                          \
	int c = fgetc( splitin );                              \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}


#include "fsc2.h"


/* local functions */

static int splitlex( void );
static bool section_parser( int section );


%}

FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

DEV         ^[\t ]*DEV(ICE)?S?:
ASS         ^[\t ]*((ASS(IGNMENT)?)|(ASSIGNMENTS)):
VAR         ^[\t ]*VAR(IABLE)?S?:
PHAS        ^[\t ]*PHA(SES)?:
PREP        ^[\t ]*PREP(ARATION)?S?:
EXP         ^[\t ]*EXP(ERIMENT)?:


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/

			/* handling of file name lines */
{FILE}      {
				*( splittext + splitleng - 1 ) = '\0';
				T_free( Fname );
				Fname = T_strdup( splittext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( splittext + splitleng - 1 ) = '\0';
				Lc = atol( splittext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		{
				printf( "%s", splittext + 2 );
				return FAIL;
			}

			/* handling of DEVICES: label */
{DEV}		{
				if ( ! section_parser( DEVICES_SECTION ) )
					return FAIL;
			}

			/* handling of ASSIGNMENTS: label */
{ASS}		{
				if ( ! section_parser( ASSIGNMENTS_SECTION ) )
					return FAIL;
			}

			/* handling of VARIABLES: label */
{VAR}		{
				if ( ! section_parser( VARIABLES_SECTION ) )
					return FAIL;
			}

			/* handling of PHASES: label */
{PHAS}      {
				if ( ! section_parser( PHASES_SECTION ) )
					return FAIL;
			}

			/* handling of PREPARATIONS: label */
{PREP}      {
				if ( ! section_parser( PREPARATIONS_SECTION ) )
					return FAIL;
			}

			/* handling of EXPERIMENT: label */
{EXP}       {
				if ( ! section_parser( EXPERIMENT_SECTION ) )
					return FAIL;
			}

			/* handling of unknown label */
.+":"       {
				eprint( FATAL, SET, "Unknown label: `%s'.\n", splittext );
				return FAIL;
			}

[\t \n]+    /* dump white space */

			/* handling of invalid input */
.   		{
				eprint( FATAL, SET, "Missing section label at start of "
                        "file.\n" );
				return FAIL;
			}

<<EOF>>     return OK;


		/*----------------------*/
%%		/*     End of rules     */
		/*----------------------*/


bool scan_main( char *file )
{
	char *cmd;
	bool split_error;
	struct sigaction sact;
	struct sigaction oact;


	fsc2_assert( file != NULL && *file != '\0' );

	/* Forget everything about previous tests */

	clean_up( );

	/* Parse the function and the device name data base */

	if ( ! functions_init( ) || ! device_list_parse( ) )
		return FAIL;

	/* Filter input through "fsc2_clean" (expected to be in `bindir') */

	cmd = get_string( "%s%sfsc2_clean %s", bindir, slash( bindir ), file );

	/* Setting the program length to -1 means that no EXPERIMENT section has
	   been found yet. */

	prg_length = -1;

	sact.sa_handler = SIG_IGN;
	sigaction( SIGCHLD, &sact, &oact );

	splitin = popen( cmd, "r" );

	/* Parse the predigested EDL file, i.e. what we get back from fsc2_clean */

	split_error = ( bool ) splitlex( );

	pclose( splitin );
	sigaction( SIGCHLD, &oact, NULL );
	T_free( cmd );
	return split_error;
}


/*--------------------------------------------------------------------*/
/* This routine is the central distributor that starts the different  */
/* parsers for all of the sections in the EDL file. It is called once */
/* from splitlex(), which just finds the very first section label and */
/* passes it to this routine. Here the appropriate lexers are called, */
/* which return the next label if they finish successfully. This new  */
/* label is used to call the next lexer. On failure several common    */
/* errors are caught (if the lexer didn't do it by itself) and the    */
/* failure is signaled back to splitlex(). On success we only return  */
/* to splitlex() end of the input file(s).                            */
/* ->                                                                 */
/*    * number (label) of first section found by splitlex()           */
/* <-                                                                 */
/*    * flag for success (OK) or failure (FAIL)                       */
/*--------------------------------------------------------------------*/

bool section_parser( int section )
{
	static int s;


	s = section;

	TRY
	{
		do
		{
			/* First run the appropriate lexer/parser combination */

			switch ( s )
			{
				case DEVICES_SECTION :
					s = devices_parser( splitin );
					break;
	
				case ASSIGNMENTS_SECTION :
					s = assignments_parser( splitin );
					break;
	
				case VARIABLES_SECTION :
					s = variables_parser( splitin );
					break;
	
				case PHASES_SECTION :
					s = phases_parser( splitin );
					break;
	
				case PREPARATIONS_SECTION :
					s = preparations_parser( splitin );
					break;
	
				case EXPERIMENT_SECTION :
					s = experiment_parser( splitin );
					break;
	
				default :              /* this should never happen, but ... */
					fsc2_assert( 1 == 0 );
					break;
			}
	
		} while ( s != NO_SECTION );

		/* Finally, we have to do all the checks that only can be done
		   after the EDL file has been completely parsed */

		TRY_SUCCESS;
		return OK;
	}
	CATCH( SYNTAX_ERROR_EXCEPTION )
	{
		eprint( FATAL, SET, "Syntax error.\n" ); 
		return FAIL;
	}
	CATCH( MISSING_SEMICOLON_EXCEPTION )
	{		
		eprint( FATAL, SET, "Missing semicolon before (or on) this line.\n" );
		return FAIL;
	}

	return FAIL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
