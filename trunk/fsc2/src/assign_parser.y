/*****************************************************************/
/* This is the parser for the ASSIGNMENTS section of an EDL file */
/*****************************************************************/


%{

#include "fsc2.h"

extern int assignlex( void );

/* locally used functions */

int assignerror( const char *s );

/* locally used global variables */

int Channel_Type;

/* externally (in assign_lexer.flex) defined global variables */ 

%}


%union {
	long   lval;
	double dval;
}


%token SECTION_LABEL         /* new section label */
%token MW_TOKEN              /* MICRO_WAVE */
%token TWT_TOKEN             /* TRAVELING_WAVE_TUBE */
%token TWT_GATE_TOKEN        /* TRAVELING_WAVE_TUBE_GATE */
%token DET_TOKEN             /* DETECTION */
%token DET_GATE_TOKEN        /* DETECTION_GATE */
%token RF_TOKEN              /* RADIO_FREQUENCY */
%token RF_GATE_TOKEN         /* RADIO_FREQUENCY_GATE */
%token PHX_TOKEN             /* PHASE_X */
%token PHY_TOKEN             /* PHASE_Y */
%token POD_TOKEN             /* POD */
%token CH_TOKEN              /* CHANNEL */
%token DEL_TOKEN             /* DELAY */
%token INV_TOKEN             /* INVERTED */
%token NS_TOKEN              /* nsec */
%token US_TOKEN              /* usec */
%token MS_TOKEN              /* msec */
%token S_TOKEN               /* sec */
%token <lval> INT_TOKEN
%token <dval> FLOAT_TOKEN

%type <dval> time unit


%%


input:   /* empty */
       | input line                   { Channel_Type =
											PULSER_CHANNEL_NO_TYPE; }
       | input SECTION_LABEL          { YYACCEPT; }
       | input ';'
;


/* A (non-empty) line has to start with one of the channel keywords */

line:    keywd pcd ';'
       | keywd pcd SECTION_LABEL      { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | keywd pcd keywd error ';'    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | error ';'                    { THROW ( SYNTAX_ERROR_EXCEPTION ); }
;


keywd:   MW_TOKEN                     { Channel_Type = PULSER_CHANNEL_MW; }
	   | TWT_TOKEN                    { Channel_Type = PULSER_CHANNEL_TWT; }
       | TWT_GATE_TOKEN               { Channel_Type = 
											PULSER_CHANNEL_TWT_GATE; }
       | DET_TOKEN                    { Channel_Type = PULSER_CHANNEL_DET; }
	   | DET_GATE_TOKEN               { Channel_Type =
											PULSER_CHANNEL_DET_GATE; }
       | RF_TOKEN                     { Channel_Type = PULSER_CHANNEL_RF; }
	   | RF_GATE_TOKEN                { Channel_Type =
											PULSER_CHANNEL_RF_GATE; }
       | PHX_TOKEN                    { Channel_Type = 
											PULSER_CHANNEL_PHASE_X; }
       | PHY_TOKEN                    { Channel_Type =
											PULSER_CHANNEL_PHASE_Y; }
;


/* Pod and channel asignments consists of the POD and CHANNEL keyword,
   followed by the pod or channel number(s), and, optionally, a DELAY keyword
   followed by the delay time and the INVERTED keyword. The sequence of the
   keywords is arbitrary. */


pcd:    /* empty */
      | pcd pod
      | pcd ch
      | pcd del
      | pcd INV_TOKEN                { assign_inv_channel( Channel_Type ); }
;


pod:    POD_TOKEN INT_TOKEN          { assign_pod( Channel_Type, $2 ); }
;

ch:     CH_TOKEN INT_TOKEN           { assign_channel( Channel_Type, $2 ); }
        ch1
;

ch1:    /* empty */
      | ch1 INT_TOKEN                { assign_channel( Channel_Type, $2 ); }
;

del:    DEL_TOKEN time               { set_pod_delay( Channel_Type, $2 ); }
;

time:   INT_TOKEN unit               { $$ = ( double ) $1 * $2; }
      | FLOAT_TOKEN unit             { $$ = $1 * $2; }
;									 
									 
unit:   /* empty */                  { $$ = 1.0; }
      | NS_TOKEN                     { $$ = 1.0; }
      | US_TOKEN                     { $$ = 1.0e3; }
      | MS_TOKEN                     { $$ = 1.0e6; }
      | S_TOKEN                      { $$ = 1.0e9; }
;

%%


int assignerror ( const char *s )
{
	return fprintf( stderr, "%s:%ld: %s in ASSIGNMENTS section.\n",
					Fname, Lc, s );
}
