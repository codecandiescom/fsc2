/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive stack nounput

%{

#include "fsc2.h"

static long Lc;
static char *Fname;
static long Comm_Lc;
static bool Eol;

int devices_listlex( void );

%}

REM1     ^[\t ]*"//".*\n
REM2     "//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n

INT      [+-]?[0-9]+

WLWS     ^[\t ]*\n
LWS      ^[\t ]+
WS       [\t ]+
TWS      [\t ]+\n

IDENT    [A-Za-z_0-9]+

%x      comm


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}		Lc++;

			/* handling of C++ style comment not spanning a whole line */
{REM2}		{
				Lc++;
				Eol = SET;
			}

			/* handling of C style comment */
{REM3}		{
				Comm_Lc = Lc;
				yy_push_state( comm );
			}

<comm>{

{REM4}		/* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

			/* end of line character in a comment */
\n			{
				Lc++;
				Eol = SET;
			}

			/* handling of EOF within a comment -> fatal error */
<<EOF>>     {
				eprint( FATAL, "%s: End of device data base `%s' in comment "
						"starting at line %ld\n.", Fname, Comm_Lc );
				THROW( DEVICES_EXCEPTION );
			}

			/* end of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* end of comment and end of line */
{EREM2}     {
				Eol = SET;
				Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     {  /* End of comment without start */
				eprint( FATAL, "End of comment found at line %ld in device "
						"data base `%s'\n.", Lc, Fname );
				THROW( DEVICES_EXCEPTION );
			}

			/* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      Lc++;

{LWS}       /* dumps of leading white space */

			/* dumps trailing white space */
{TWS}       {
				Lc++;
				Eol = SET;
			}

{WS}        /* dum (other) white space */

			/* writes out EOL character */
\n			{
				Lc++;
				Eol = SET;
			}

,           /* skip commas */
;           /* skip semicolon */

{IDENT}     return 1;

.           {
				eprint( FATAL, "Syntax error in devices data base `%s' at "
						"line %ld.\n", Fname, Lc );
				THROW( DEVICES_EXCEPTION );
			}

<<EOF>>		return( 0 );



		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


bool device_list_parse( void )
{
	Device_Name *new_device_name;


	Fname = get_string_copy( "Devices" );

	if ( ( devices_listin = fopen( Fname, "r" ) ) == NULL )
	{
		eprint( FATAL, "Can't open device data base `%s'.\n", Fname );
		free( Fname );
		THROW( DEVICES_EXCEPTION );
	}

	Lc = 1;
	Eol = SET;

	TRY
	{
		while ( devices_listlex( ) )
		{
			new_device_name = T_malloc( sizeof( Device_Name ) );
			new_device_name->name = get_string_copy( devices_listtext );
			string_to_lower( new_device_name->name );
			new_device_name->next = Device_Name_List;
			Device_Name_List = new_device_name;
		}
   		TRY_SUCCESS;
	}
	CATCH( OUT_OF_MEMORY_EXCEPTION )
	{
		fclose( devices_listin );
		free( Fname );
		delete_device_name_list( );
		return FAIL;
	}

	fclose( devices_listin );
	return OK;
}
