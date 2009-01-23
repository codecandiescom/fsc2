/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


/************************************************************/
/* This is the parser for the PHASES section of an EDL file */
/************************************************************/


%{

#include "fsc2.h"

extern int phaseslex( void );    /* defined in phases_lexer.l */
extern char *phasestext;         /* defined in phases_lexer.l */

/* locally used functions */

static void phaseserror( const char * s );


/* locally used global variables */

static Phs_Seq_T *Phase_seq;


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
       | input line                   { Phase_seq = NULL; }
       | input SECTION_LABEL          { YYACCEPT; }
       | input ';'                    { Phase_seq = NULL; }
;

line:    acq ';'
       | acq phase                    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | acq SECTION_LABEL            { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phase ';'
       | phase acq                    { THROW( MISSING_SEMICOLON_EXCEPTION ); }
       | phase SECTION_LABEL          { THROW( MISSING_SEMICOLON_EXCEPTION ); }
;

acq:     AS_TOKEN A_TOKEN             { acq_seq_start( $1, $2 ); }
         a_list
       | AS_TOKEN                     { acq_miss_list( ); }
;

a_list:  /* empty */
       | a_list A_TOKEN               { acq_seq_cont( $2 ); }
;

phase:   PS_TOKEN                     { Phase_seq = phase_seq_start( $1 ); }
         P_TOKEN                      { phases_add_phase( Phase_seq, $3 ); }
         p_list
       | PS_TOKEN                     { phase_miss_list( Phase_seq ); }
;

p_list:  /* empty */
       | p_list P_TOKEN               { phases_add_phase( Phase_seq, $2 ); }
;


%%


/*----------------------------------------------------*
 *----------------------------------------------------*/

static void
phaseserror( const char * s  UNUSED_ARG )
{
    if ( *phasestext == '\0' )
        print( FATAL, "Unexpected end of file in PHASES section.\n" );
    else
        print( FATAL, "Syntax error near token '%s'.\n", phasestext );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
