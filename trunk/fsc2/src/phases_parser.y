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

/************************************************************/
/* This is the parser for the PHASES section of an EDL file */
/************************************************************/


%{

#include "fsc2.h"

extern int phaseslex( void );

/* locally used functions */

static void phaseserror( const char *s );


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


static void phaseserror ( const char *s )
{
	s = s;                    /* avoid compiler warning */

	if ( *phasestext == '\0' )
		eprint( FATAL, SET, "Unexpected end of file in PHASES section.\n" );
	else
		eprint( FATAL, SET, "Syntax error near token `%s'.\n", phasestext );
	THROW( EXCEPTION );
}
