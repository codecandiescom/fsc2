/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap stack

%{

#include "fsc2.h"

long hp8647a_Lc,
	 hp8647a_Comm_Lc;

%}

REM1     ^[\t ]*"//".*\n
REM2     [\t ]*"//".*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n
WLWS     ^[\t ]*\n
WS       [\t ]+
TWS      [\t ]+\n

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

%x      comm


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}		hp8647a_Lc++;

			/* handling of C++ style comment not spanning a whole line */
{REM2}		hp8647a_Lc++;

			/* handling of C style comment */
{REM3}		{
				hp8647a_Comm_Lc = hp8647a_Lc;
				yy_push_state( comm );
			}

<comm>{

{REM4}		/* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

			/* end of line character in a comment */
\n			hp8647a_Lc++;

			/* handling of EOF within a comment -> fatal error */
<<EOF>>		THROW( EOF_IN_COMMENT_EXCEPTION );

			/* end of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* end of comment and end of line */
{EREM2}     {
				hp8647a_Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT )

			/* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      hp8647a_Lc++;

			/* dumps trailing white space */
{TWS}       hp8647a_Lc++;

			/* dump all other forms of white */
{WS}        

			/* count lines */
\n			hp8647a_Lc++;

{INT}       return hp8647atext;
{FLOAT}     return hp8647atext;

<<EOF>>		return NULL;

.           THROW( INVALID_INPUT_EXCEPTION );


		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


hp8647a_reader( FILE *fp, const char *dev_name )
{
	static bool is_restart = UNSET;
	char *ret;


	if ( ( hp8647ain = fp ) == NULL )
	{
		eprint( FATAL, "%s: Internal error while reading attenuation table.\n",
				dev_name );
		THROW( EXCEPTION );
	}

	/* Keep the lexer happy... */

	if ( is_restart )
	    hp8647arestart( hp8647ain );
	else
		 is_restart = SET;

	hp8647a_Lc = 1;

	TRY {
		while( ( ret = hp8647alex( ) ) != NULL )
			;
		TRY_SUCCESS;
	}
}
