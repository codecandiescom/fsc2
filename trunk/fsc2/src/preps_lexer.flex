/*****************************************************************/
/* This is the lexer for the PREPARATIONS section of an EDL file */
/*****************************************************************/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap case-insensitive nounput

%{

/* We have to make the lexer read the input byte by byte since it might be
   called from within another lexer and wants to use the same input - otherwise
   it would also read in input into its internal buffer belonging to the
   calling lexer  */


#define YY_INPUT( buf, result, max_size )                  \
{                                                          \
	int c = fgetc( prepsin );                              \
	result = ( c == EOF ) ? YY_NULL : ( buf[ 0 ] = c, 1 ); \
}

#include "fsc2.h"
#include "preps_parser.h"


int prepslex( void );
extern void prepsparse( void );

int extr_pnum( char *txt );


/* locally used global variables */

int Preps_Next_Section;


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

ESTR        \x5.*\x3\n.*\n
STR         \x5.*\x6

MW          M(ICRO)?_?W(AVE)?:?
TWT         T(RAVELING)?_?W(AVE)?_?T(UBE)?:?
TWT_GATE    T(RAVELING)?_?W(AVE)?_?T(UBE)?_?G(ATE)?:?
DET         DET(ECTION)?:?
DET_GATE    DET(ECTION)?_?G(ATE)?:?
RF          R(ADIO)?_?F(REQUENCY)?:?
RF_GATE     R(ADIO)?_?F(REQUENCY)?_?G(ATE)?:?

INT         [0-9]+
EXPO        [EDed][+-]?{INT}
FLOAT       ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})

P           P(ULSE)?_?{INT}

F           F(UNC(TION)?)?
S           S(TART)?
L			L(EN(GTH)?)?
DS          D(EL(TA)?)?_?S(TART)?
DL          D(EL(TA)?)?_?L(EN(GTH)?)?
PH          PH(ASE(SEQ(UENCE)?)?)?_?{INT}?
ML          M(AX(IMUM)?)?_?L(EN(GTH)?)?
RP          R(EPL(ACEMENT)?)?_?P((ULSE)?S?)?


WS          [\n \t]+

IDENT       [A-Za-z]+[A-Za-z0-9_]*

UNREC       [^\n \t;,\(\)\=\+\-\*\/\[\]\%\^:]+


		/*---------------*/
%%		/*     Rules     */
		/*---------------*/


			/* handling of file name lines */
{FILE}      {
				*( prepstext + prepsleng - 1 ) = '\0';
				if ( Fname != NULL )
					free( Fname );
				Fname = get_string_copy( prepstext + 2 );
			}

			/* handling of line number lines */
{LNUM}		{
				*( prepstext + prepsleng - 1 ) = '\0';
				Lc = atol( prepstext + 2 );
			}

			/* handling of error messages from the cleaner */
{ERR}		THROW( CLEANER_EXCEPTION );
{ESTR}		{
				prepstext = strchr( prepstext, '\x03' );
				THROW( CLEANER_EXCEPTION );
			}

			/* handling of ASSIGNMENTS: labels */
{ASS}		{
				Preps_Next_Section = ASSIGNMENTS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of DEFAULTS: labels */
{DEF}		{
				Preps_Next_Section = DEFAULTS_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of VARIABLES: labels */
{VAR}		{
				Preps_Next_Section = VARIABLES_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of PHASES: labels */
{PHAS}		{
				Preps_Next_Section = PHASES_SECTION;
				return( SECTION_LABEL );
			}

			/* handling of EXPERIMENT: labels */
{EXP}		{
				Preps_Next_Section = EXPERIMENT_SECTION;
				return( SECTION_LABEL );
			}

{INT}       {
				prepslval.lval = atol( prepstext );
                return( INT_TOKEN );
            }

{FLOAT}     {
                prepslval.dval = atof( prepstext );
                return( FLOAT_TOKEN );
            }

            /* handling of string constants (to be used as format strings in
			   the print() function only */
{STR}       {
				prepstext[ strlen( prepstext ) - 1 ] = '\0';
				prepslval.sptr = prepstext + 1;
				return( STR_TOKEN );
			}


			/* all pulse related keywords... */

{P}":"?     {
			    Cur_Pulse = pulse_new( extr_pnum( prepstext ) );
				return( P_TOK );
			}

{F}         return( F_TOK );
{S}			return( S_TOK );
{L}			return( L_TOK );
{DS}        return( DS_TOK );
{DL}        return( DL_TOK );
{PH}        return( PH_TOK );
{ML}        return( ML_TOK );
{RP}        return( RP_TOK );

			/* combinations of pulse and property, e.g. `P3.LEN' */

{P}?"."{F}  {
				if ( *prepstext == '.' )
					prepslval.lval = pulse_get_by_addr( Cur_Pulse, P_FUNC );
				else
					prepslval.lval = pulse_get_by_num( extr_pnum( prepstext ),
													   P_FUNC );
				return( INT_TOKEN );
            }

{P}?"."{S}  {
				if ( *prepstext == '.' )
					prepslval.lval = pulse_get_by_addr( Cur_Pulse, P_POS );
				else
					prepslval.lval = pulse_get_by_num( extr_pnum( prepstext ),
													   P_POS );
				return( INT_TOKEN );
            }

{P}?"."{L}  {
				if ( *prepstext == '.' )
					prepslval.lval = pulse_get_by_addr( Cur_Pulse, P_LEN );
				else
					prepslval.lval = pulse_get_by_num( extr_pnum( prepstext ),
													   P_LEN );
				return( INT_TOKEN );
            }

{P}?"."{DS} {
				if ( *prepstext == '.' )
					prepslval.lval = pulse_get_by_addr( Cur_Pulse, P_DPOS );
				else
					prepslval.lval = pulse_get_by_num( extr_pnum( prepstext ),
													   P_DPOS );
				return( INT_TOKEN );
            }

{P}?"."{DL} {
				if ( *prepstext == '.' )
					prepslval.lval = pulse_get_by_addr( Cur_Pulse, P_DLEN );
				else
					prepslval.lval = pulse_get_by_num( extr_pnum( prepstext ),
													   P_DLEN );
				return( INT_TOKEN );
            }

{P}?"."{ML} {
				if ( *prepstext == '.' )
					prepslval.lval = pulse_get_by_addr( Cur_Pulse, P_MAXLEN );
				else
					prepslval.lval = pulse_get_by_num( extr_pnum( prepstext ),
													   P_MAXLEN );
				return( INT_TOKEN );
            }


{MW}        {
				prepslval.lval = PULSER_CHANNEL_MW;
				return( FUNC_TOK );
			}
{TWT}       {
				prepslval.lval = PULSER_CHANNEL_TWT;
				return( FUNC_TOK );
			}
{TWT_GATE}  {
				prepslval.lval = PULSER_CHANNEL_TWT_GATE;
				return( FUNC_TOK );
			}
{DET}       {
				prepslval.lval = PULSER_CHANNEL_DET;
				return( FUNC_TOK );
			}
{DET_GATE}  {
				prepslval.lval = PULSER_CHANNEL_DET;
				return( FUNC_TOK );
			}
{RF}        {
				prepslval.lval = PULSER_CHANNEL_RF;
				return( FUNC_TOK );
			}
{RF_GATE}   {
				prepslval.lval = PULSER_CHANNEL_RF_GATE;
				return( FUNC_TOK );
			}


			/* handling of function, variable and array identifiers */
{IDENT}     {
				int acc;

				/* special treatment for calls of print() function */

				if ( ! strcmp( prepstext, "print" ) )
				    return( PRINT_TOK );

				/* first check if the identifier is a function name */

				prepslval.vptr = func_get( prepstext, &acc );
				if ( prepslval.vptr != NULL )
				{
					if ( acc != ACCESS_ALL_SECTIONS )
					{
						eprint( FATAL, "%s:%ld: Function `%s' can't be used "
								"in PREPARATIONS section.\n",
								 Fname, Lc, prepstext );
						THROW( SYNTAX_ERROR_EXCEPTION );
					}
					return( FUNC_TOKEN );
				}

				/* if it's not a function it should be a variable */

				if ( ( prepslval.vptr = vars_get( prepstext ) ) == NULL )
					 THROW( ACCESS_NONEXISTING_VARIABLE );

				return( VAR_TOKEN );
			}

			/* stuff used with functions, arrays and math */

"=="        return( EQ );        /* equal */                  
"<"         return( LT );        /* less than */              
"<="        return( LE );        /* less than or equal */     
">"         return( GT );        /* greater than */           
">="        return( GE );        /* greater than or equal */  
"="         return( '=' );       /* assignment operator */    
"["         return( '[' );       /* start of array indices */ 
"]"         return( ']' );       /* end of array indices */   
","         return( ',' );       /* list separator */         
"("         return( '(' );       /* start of function argument list */
")"         return( ')' );       /* end of function argument list */
"+"         return( '+' );       /* addition operator */      
"-"         return( '-' );       /* subtraction operator or unary minus */
"*"         return( '*' );       /* multiplication operator */
"/"         return( '/' );       /* division operator */      
"%"         return( '%' );       /* modulo operator */        
"^"         return( '^' );       /* exponentiation operator */

       /* quasi-assignment operator for pulse properties */

":"         return( ':' );

			/* handling of end of statement character */
";"			return( ';' );

{WS}        /* skip white space */

"\x4nsec"   return( NS_TOKEN );
"\x4usec"   return( US_TOKEN );
"\x4msec"   return( MS_TOKEN );
"\x4sec"    return( S_TOKEN );

			/* handling of invalid input */
{UNREC}     {
				eprint( FATAL, "%s:%ld: Invalid input in PREPARATIONS section:"
						" `%s'\n", Fname, Lc, prepstext );
				return( 0 );
			}

<<EOF>>	    {
				Preps_Next_Section = NO_SECTION;
				return( 0 );
			}


		/*----------------------*/
%%		/*     End of Rules     */
		/*----------------------*/


int preparations_parser( FILE *in )
{
	if ( compilation.sections[ PREPARATIONS_SECTION ] )
	{
		eprint( FATAL, "%s:%ld: Multiple instances of PREPARATIONS section "
		        "label.\n", Fname, Lc );
		return( FAIL );
	}
	compilation.sections[ PREPARATIONS_SECTION ] = SET;

	Preps_Next_Section = OK;

	prepsin = in;

	TRY
		prepsparse( );
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s:%ld: Invalid input in PREPARATIONS section: `%s'\n",
				Fname, Lc, prepstext );
		return( FAIL );
    }
	CATCH( CLEANER_EXCEPTION )
	{
		eprint( FATAL, "%s", prepstext + 2 );
		return( FAIL );
	}
	CATCH( ACCESS_NONEXISTING_VARIABLE )
	{
		eprint( FATAL, "%s:%ld: Variable `%s' has never been declared.\n",
				Fname, Lc, prepstext );
		return( FAIL );
	}
	CATCH( PREPARATIONS_EXCEPTION )
		return( FAIL );

	return( Preps_Next_Section );
}



/*----------------------------------------------*/
/* Extracts the pulse number from a pulse name. */
/* ->                                           */
/*    * pulse name string                       */
/* <-                                           */
/*    * pulse number or -1 on error             */
/*----------------------------------------------*/

int extr_pnum( char *txt )
{
	char *tp = get_string_copy( txt );
	char *t = tp;
	int ret;


	while ( *t && *t != '.' && *t != ':' )
		t++;

	if ( *t == '.' || *t == ':' )
	    *txt = '\0';

	while( isdigit( *--t ) )
		;

	if ( isdigit( *++t ) )
	    ret = atoi( t );
	else
		ret = -1;

	free( tp );
	return( ret );
}
