/*
  $Id$

  Copyright (C) 2003 Jens Thoms Toerring

  Library for Rulbus (Rijksuniversiteit Leiden BUS)

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  The library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the package; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.

  To contact the author send email to
  Jens.Toerring@physik.fu-berlin.de
*/


%{

#include "rulbus_lib.h"

extern int yylex( void );

extern RULBUS_CARD_LIST *rulbus_card;
extern int rulbus_num_cards;
extern char *rulbus_dev_file;

static int rack;
static int card_no;
static int ret;

static int set_dev_file( const char *name );
static int new_card( void );
static int set_name( const char *name );
static int set_addr( unsigned int addr );
static int set_type( unsigned int type );
static int set_rack( void );
static void yyerror( const char *s );

%}

%union
{
    unsigned int ival;
    char *sval;
}

%token FILE_TOKEN RACK_TOKEN CARD_TOKEN NAME_TOKEN TYPE_TOKEN ADDR_TOKEN
%token NUM_TOKEN CARD_TYPE_TOKEN STR_TOKEN ERR_TOKEN

%type <ival> NUM_TOKEN CARD_TYPE_TOKEN
%type <sval> STR_TOKEN


%%

line:     /* empty */
	    | line file
	    | line rack
        | error                           { return RULBUS_STX_ERR; }
        | ERR_TOKEN                       { return RULBUS_STX_ERR; }
;


file:     FILE_TOKEN sep1 STR_TOKEN sep2  { if ( ( ret = set_dev_file( $3 ) ) )
								  				return ret; }
;


rack:     RACK_TOKEN sep1				  { card_no = rulbus_num_cards; }
		  '{' rdata '}' sep2			  { if ( ( ret = set_rack( ) ) )
												return ret; }
;


rdata:    /* empty */
        | rdata ADDR_TOKEN sep1 NUM_TOKEN sep2
										  { if ( rack != -1 )
								  				return RULBUS_RCK_DUP;
							  				rack = ( unsigned char ) $4; }
		| rdata CARD_TOKEN sep1 '{'		  { if ( ( ret =  new_card( ) ) )
												return ret; }
		  cdata '}' sep2
;


cdata:    /* empty */
		| cdata NAME_TOKEN sep1 STR_TOKEN sep2
										  { if ( ( ret =  set_name( $4 ) )  )
												return ret; }
		| cdata ADDR_TOKEN sep1 NUM_TOKEN sep2
										  { if ( ( ret =  set_addr( $4 ) ) )
												return ret; }
		| cdata CARD_TYPE_TOKEN sep1 NUM_TOKEN sep2
										  { if ( ( ret =  set_type( $4 ) ) )
												return ret; }
;


sep1:     /* empty */
        | '='
        | ':'
;


sep2:     /* empty */
        | ','
        | ';'
;


%%


/*----------------------------------------------------------------------------
 * Function for setting the name of the device file - can only be called once
 *---------------------------------------------------------------------------*/

static int set_dev_file( const char *name )
{
	if ( rulbus_dev_file != NULL )
	{
		free( rulbus_dev_file );
		return RULBUS_DVF_CNT;
	}

	if ( ( rulbus_dev_file = strdup( name ) ) == NULL )
		return RULBUS_NO_MEM;

	strcpy( rulbus_dev_file, name );
	return RULBUS_OK;
}


/*------------------------------------------------*
 * Function to be called when a new card is found
 *------------------------------------------------*/

static int new_card( void )
{
	RULBUS_CARD_LIST *tmp;


	/* Check that there aren't too many cards */

	if ( rulbus_num_cards >= RULBUS_MAX_CARDS )
		return RULBUS_MAX_CRD;

	/* Set up a new structure for the card */

	if ( ( tmp = realloc( rulbus_card, ( rulbus_num_cards + 1 )
						  			   * sizeof *tmp ) ) == NULL )
		return RULBUS_NO_MEM;

	rulbus_card = tmp;

	rulbus_card[ rulbus_num_cards ].name = NULL;
	rulbus_card[ rulbus_num_cards ].addr = RULBUS_INVALID_ADDR;
	rulbus_card[ rulbus_num_cards++ ].type = -1;

	return RULBUS_OK;
}


/*------------------------------------------------------------*
 * Function that gets called when the name of a card is found 
 *------------------------------------------------------------*/

static int set_name( const char *name )
{
	int i;


	/* Check that the name hasn't been used before */

	for ( i = 0; i < rulbus_num_cards - 1; i++ )
		if ( rulbus_card[ i ].name &&
			 ! strcpy( rulbus_card[ i ].name, name ) )
			return RULBUS_NAM_DUP;

	/* Check that the card hasn't already been assigned a name */

	if ( rulbus_card[ rulbus_num_cards - 1 ].name != NULL )
		return RULBUS_DUP_NAM;

	if ( ( rulbus_card[ rulbus_num_cards - 1 ].name = strdup( name ) )
		 															  == NULL )
		return RULBUS_NO_MEM;

	strcpy( rulbus_card[ rulbus_num_cards - 1 ].name, name );
	return RULBUS_OK;
}


/*--------------------------------------------------------*
 * Function that gets called when a card address is found
 *--------------------------------------------------------*/

static int set_addr( unsigned int addr )
{
	/* Check that the address is reasonable */

	if ( addr < RULBUS_MIN_CARD_ADDR || addr > RULBUS_MAX_CARD_ADDR )
		return RULBUS_CRD_ADD;

	/* Check that the card hasn't already been assigned an address */

	if ( rulbus_card[ rulbus_num_cards - 1 ].addr != RULBUS_INVALID_ADDR )
		return RULBUS_ADD_DUP;

	rulbus_card[ rulbus_num_cards - 1 ].addr = ( unsigned char ) addr;
	return RULBUS_OK;
}


/*-----------------------------------------------------*
 * Function that gets called when a card type is found 
 *-----------------------------------------------------*/

static int set_type( unsigned int type )
{
	/* Check that the card hasn't already been assigned a type */

	if ( rulbus_card[ rulbus_num_cards - 1 ].type != -1 )
		return RULBUS_TYP_DUP;

	rulbus_card[ rulbus_num_cards - 1 ].type = type;
	return RULBUS_OK;
}


/*-----------------------------------------------------------------------*
 * Function that gets called when the end of a rack description is found
 *-----------------------------------------------------------------------*/

static int set_rack( void )
{
	int i;


	/* Check that a rack address was set and that it is reasonable */

	if ( rack == -1 )
		return RULBUS_NO_RCK;

	if ( rack > RULBUS_MAX_RACK_NO )
		return RULBUS_INV_RCK;

	/* Check that there aren't too many cards for the rack */

	if ( rulbus_num_cards - card_no > RULBUS_MAX_CARDS_IN_RACK )
		return RULBUS_CRD_CNT;

	/* Set the rack number for all of the cards */

	for ( i = card_no; i < rulbus_num_cards; i++ )
	{
		rulbus_card[ i ].rack = ( unsigned char ) rack;
		rulbus_card[ i ].in_use = UNSET;
	}

	rack = -1;

	return RULBUS_OK;
}


/*-------------------------------------------------------------------*
 * flex requires this function to exist...
 *-------------------------------------------------------------------*/

static void yyerror( const char *s )
{
	s = s;
}
