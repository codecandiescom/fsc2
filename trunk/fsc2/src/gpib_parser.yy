/*
  $Id$

  Copyright (c) 2001 Jens Thoms Toerring

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


%{


#include "fsc2.h"
#include "gpib_if.h"


extern int gpiblex( void );
void gpiberror( const char *s );

static GPIB_Device dev;

#define GPIB_DEF_TIMO T1s

%}


%union
{
	bool bval;
	int  ival;
	char *sval;
}


%token DEVICE_TOKEN NAME_TOKEN PAD_TOKEN SAD_TOKEN EOS_TOKEN TIMO_TOKEN
%token REOS_TOKEN XEOS_TOKEN BIN_TOKEN EOT_TOKEN MASTER_TOKEN FILE_TOKEN
%token ERR_TOKEN BOOL_TOKEN TIME_TOKEN NUM_TOKEN STR_TOKEN

%type <bval> BOOL_TOKEN bool
%type <ival> TIME_TOKEN NUM_TOKEN
%type <sval> STR_TOKEN

%%

line:   /* empty */
      | line device                         { if ( gpib_dev_setup( &dev ) < 0 )
		                                          YYABORT;
                                            }
	  | error                               { YYABORT; }
;

device: DEVICE_TOKEN '{'                    {
		                                      *dev.name   = '\0';
                                              dev.pad     = -1;
									          dev.sad     = 0;
                                              dev.eos     = '\n';
                                              dev.eosmode = 0;
			                                  dev.flags   = 0;
									          dev.timo    = GPIB_DEF_TIMO;
                                            }
        args '}' 
;

args:   /* empty */
      | args NAME_TOKEN sep1 STR_TOKEN sep2  { strncpy( dev.name, $4,
														GPIB_NAME_MAX + 1 ); }
      | args PAD_TOKEN sep1 NUM_TOKEN sep2	 { dev.pad = $4; }
      | args SAD_TOKEN sep1 NUM_TOKEN sep2	 { dev.sad = $4; }
      | args EOS_TOKEN sep1 NUM_TOKEN sep2	 { dev.eos = $4; }
      | args TIMO_TOKEN sep1 NUM_TOKEN sep2  { dev.timo = $4; }
      | args TIMO_TOKEN sep1 TIME_TOKEN sep2 { dev.timo = $4; }
	  | args REOS_TOKEN sep1 bool sep2		 { dev.eosmode |= $4 * REOS; }
	  | args XEOS_TOKEN sep1 bool sep2		 { dev.eosmode |= $4 * XEOS; }
      | args BIN_TOKEN sep1 bool sep2		 { dev.eosmode |= $4 * BIN; }
      | args EOT_TOKEN sep1 bool sep2		 { dev.eosmode |= $4 * EOT; }
      | args MASTER_TOKEN sep2               { dev.flags |= IS_MASTER; }
      | args FILE_TOKEN sep1 STR_TOKEN sep2  { }
      | args ERR_TOKEN sep1 STR_TOKEN sep2   { }
;

bool:   BOOL_TOKEN                           { $$ = $1; }
      | NUM_TOKEN                            { $$ = $1 ? 1 : 0; }
;

sep1:   /* empty */
	  | '='
      | ':'
;

sep2:   /* empty */
	  | ','
      | ';'
;


%%


void gpiberror( const char *s )
{
	extern char *gpibtext;


	printf( "%s at token '%s'\n", s, gpibtext );
	ibsta |= IBERR;
}
