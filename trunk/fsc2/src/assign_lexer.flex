/****************************************************************/
/* This is the lexer for the ASSIGNMENTS section of an EDL file */
/****************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive nounput

%{

/* We declare our own input routine to make the lexer read the input byte
   by byte instead of larger chunks - since the lexer might be called by
   another lexer we thus avoid reading stuff which is to be handled by
   the calling lexer. */

#define YY_INPUT( buf, result, max_size )                  \
{                                                          \
	int c = fgetc( assignin );                             \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
#include "assign_parser.h"


int assignlex( void );
extern void assignparse( void );


/* locally used global variables */

int Assign_Next_Section;

%}


FILE        \x1\n.+\n
LNUM        \x2\n[0-9]+\n
ERR         \x3\n.+\n

ASS         ^[ \t]*ASS(IGNMENT)?S?:
DEF         ^[ \t]*DEF(AULT)?S?:
VAR         ^[ \t]*VAR(IABLE)?S?:
PHAS        ^[ \t]*PHA(SE)?S?:
PREP        ^[ \t]*PREP(ARATION)?S?:
EXP         ^[ \t]*EXP(ERIMENT)?:

WS          [\n=,:. ]+
INT         [+-]?[0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})
MW          M(ICRO)?_?W(AVE)?:?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?:?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?:?
DET         DET(ECTION)?:?
DET_GATE    DET(ECTION)?_?G(ATE)?:?
RF          R(ADIO)?_?F(REQUENCY)?:?
RF_GATE     R(ADIO)?_?F(REQUENCY)?_?G(ATE)?:?
PX          PH(ASE)?_?X:?
PY          PH(ASE)?_?Y:?
DEL         ((D)|(DEL)|(DELAY)):?
POD         P(OD)?
CH          C(H(ANNEL)?)?
INV         I(NV(ERT(ED)?)?)?
UNREC       [^\n=,;:. ]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( assigntext + assignleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( assigntext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( assigntext + assignleng - 1 ) = '\0';
				Lc = atol( assigntext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		THROW( CLEANER_EXCEPTION );

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Assign_Next_Section = ASSIGNMENTS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of DEFAULTS: labels */
{DEF}		{
				Assign_Next_Section = DEFAULTS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Assign_Next_Section = VARIABLES_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Assign_Next_Section = PHASES_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of PREPARATIONS: labels */
{PREP}		{
				Assign_Next_Section = PREPARATIONS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Assign_Next_Section = EXPERIMENT_SECTION;
				return( SECTION_LABEL );
			}

{MW}        return( MW_TOKEN );

{TWT}       return( TWT_TOKEN );

{TWT_GATE}  return( TWT_GATE_TOKEN );

{DET}       return( DET_TOKEN );

{DET_GATE}  return( DET_GATE_TOKEN );

{RF}        return( RF_TOKEN );

{RF_GATE}   return( RF_GATE_TOKEN );

{PX}        return( PHX_TOKEN );

{PY}        return( PHY_TOKEN );

{POD}       return( POD_TOKEN );

{DEL}       return( DEL_TOKEN );

{CH}        return( CH_TOKEN );

{INT}       {
            	assignlval.lval = atol( assigntext );
                return( INT_TOKEN );
            }

{FLOAT}     {
            	assignlval.dval = atof( assigntext );
                return( FLOAT_TOKEN );
            }

{INV}       return( INV_TOKEN );

"\x4nsec"   return( NS_TOKEN );
"\x4usec"   return( US_TOKEN );
"\x4msec"   return( MS_TOKEN );
"\x4sec"    return( S_TOKEN );

{WS}        /* skip prettifying characters */

";"         return( ';' );            /* end of statement character */

			/* handling of invalid input */
{UNREC}     THROW( INVALID_INPUT_EXCEPTION );

<<EOF>>	    {
				Assign_Next_Section = NO_SECTION;
				return( 0 );
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int assignments_parser( FILE *in )
{
	if ( compilation.sections[ ASSIGNMENTS_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of ASSIGNMENTS section "
		        "label.\n", Fname, Lc );
		return( FAIL );
	}
	compilation.sections[ ASSIGNMENTS_SECTION ] = SET;

	Assign_Next_Section = OK;

	assignin = in;
	assign_init( );

	TRY
	{
		assignparse( );
		assign_end( );
	}
	CATCH( ASSIGNMENTS_EXCEPTION )
		return( FAIL );
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in ASSIGNMENTS section: "
				"`%s'\n", Fname, Lc, assigntext );
		return( FAIL );
    }
	CATCH( CLEANER_EXCEPTION )
	{
		eprint( FATAL, "%s", assigntext + 2 );
		return( FAIL );
	}

	return( Assign_Next_Section );
}
