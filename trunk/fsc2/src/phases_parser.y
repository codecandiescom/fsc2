/*
  $Id$
*/

/************************************************************/
/* This is the parser for the PHASES section of an EDL file */
/************************************************************/


%{

#include "fsc2.h"

extern int phaseslex( void );

/* locally used functions */

void phaseserror( const char *s );


extern char *phasestext;


/* locally used global variables */

Phase_Sequence *Phase_Seq;


%}

%union {
	long lval;
}


%token SECTION_LABEL        /* new section label */
%token <lval> AS_TOKEN      /* AQUISITION_SEQUENCE with value 0 or 1 */
%token <lval> PS_TOKEN      /* PHASE_SEQUENCE with value 0 to ... */
%token <lval> P_TOKEN       /* phase types */
%token <lval> A_TOKEN       /* acquisition signs */


%%


input:   /* empty */
       | input line                   { Phase_Seq = NULL; }
       | input SECTION_LABEL          { YYACCEPT; }
       | input ';'                    { Phase_Seq = NULL; }
;

line:    acq ';'
       | acq phase                    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | acq SECTION_LABEL            { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | acq error                    { THROW( SYNTAX_ERROR_EXCEPTION ); }
       | phase ';'
	   | phase acq                    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phase SECTION_LABEL          { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;

acq:     AS_TOKEN A_TOKEN             { acq_seq_start( $1, $2 ); }
         a_list
       | AS_TOKEN                     { acq_miss_list( ); }
;

a_list:  /* empty */
       | a_list A_TOKEN               { acq_seq_cont( $2 ) ; }
;

phase:   PS_TOKEN                     { Phase_Seq = phase_seq_start( $1 ); }
         P_TOKEN                      { phases_add_phase( Phase_Seq, $3 ); }
         p_list
       | PS_TOKEN                     { phase_miss_list( Phase_Seq ); }
;

p_list:  /* empty */
       | p_list P_TOKEN               { phases_add_phase( Phase_Seq, $2 ); }
;


%%


void phaseserror ( const char *s )
{
	s = s;                    /* stupid but avoids compiler warning */

	if ( *phasestext == '\0' )
		eprint( FATAL, "%s:%ld: Unexpected end of file in PHASES section.\n",
				Fname, Lc );
	else
		eprint( FATAL, "%s:%ld: Syntax error near token `%s'.\n",
				Fname, Lc, phasestext );
	THROW( EXCEPTION );
}
