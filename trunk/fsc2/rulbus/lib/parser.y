/*
  $Id$

  Copyright (C) 2003-2004 Jens Thoms Toerring

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

extern int rulbus_lex( void );

void rulbus_parser_init( void );

extern RULBUS_CARD_LIST *rulbus_card;
extern int rulbus_num_cards;

static int rack;                 /* address of current rack */
static RULBUS_CARD_LIST *card;   /* pointer to current card */
static int ret;                  
static bool rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR + 1 ];

static int set_dev_file( const char *name );
static int rack_addr( int addr );
static int setup_cards( void );
static int new_card( int type, char *name );
static int set_defaults( RULBUS_CARD_LIST *card );
static int set_rb8509_defaults( RULBUS_CARD_LIST *card );
static int set_rb8510_defaults( RULBUS_CARD_LIST *card );
static int set_rb8514_defaults( RULBUS_CARD_LIST *card );
static int set_rb8515_defaults( RULBUS_CARD_LIST *card );
static int set_rb_generic_defaults( RULBUS_CARD_LIST *card );
static int set_addr( int addr );
static int set_nchan( int nchan );
static int set_vpb( double vpb );
static int set_bipolar( int is_bipolar );
static int set_exttrig( int is_ext_trigger );
static int set_intr_delay( double intr_delay );
static void rulbus_error( const char *s );

%}

%union
{
    unsigned int ival;
	double dval;
    char *sval;
}

%token FILE_TOKEN RACK_TOKEN ADDR_TOKEN VPB_TOKEN POL_TOKEN NUM_TOKEN
%token CARD_TOKEN STR_TOKEN NCHAN_TOKEN ERR_TOKEN EOFC_TOKEN EXTTRIG_TOKEN
%token FLOAT_TOKEN UNIT_TOKEN IDEL_TOKEN

%type <ival> NUM_TOKEN CARD_TOKEN
%type <dval> FLOAT_TOKEN UNIT_TOKEN number unit
%type <sval> STR_TOKEN


%%


line:     /* empty */
	    | line file
	    | line rack
        | error                         { return RULBUS_CF_SYNTAX_ERROR; }
        | ERR_TOKEN                     { return RULBUS_CF_SYNTAX_ERROR; }
        | EOFC_TOKEN                    { return RULBUS_CF_EOF_IN_COMMENT; }
;

file:     FILE_TOKEN sep1 STR_TOKEN
	                                    { if ( ( ret = set_dev_file( $3 ) ) )
								  			  return ret; }
          sep2
;

rack:     RACK_TOKEN sep1       		{ rack = RULBUS_INV_RACK_ADDR; }
		  STR_TOKEN rinit sep0
;

rinit:    /* empty */
		| '{' rdata '}'                 { if ( ( ret = setup_cards( ) ) )
			                                  return ret; }
;

rdata:    /* empty */
        | rdata ADDR_TOKEN sep1 NUM_TOKEN
	                                    { if ( ( ret = rack_addr( $4 ) ) )
											  return ret; }
          sep2
		| rdata CARD_TOKEN sep1 STR_TOKEN
                                        { if ( ( ret = new_card( $2, $4 ) ) )
											  return ret; }
		  cinit sep0
;

cinit:    /* empty */
		| '{' cdata '}'                 { if ( ( ret = set_defaults( card ) ) )
			                                  return ret; }
;

cdata:    /* empty */
		| cdata ADDR_TOKEN sep1 NUM_TOKEN
										{ if ( ( ret = set_addr( $4 ) ) )
											  return ret; }
          sep2
		| cdata NCHAN_TOKEN sep1 NUM_TOKEN
										{ if ( ( ret = set_nchan( $4 ) ) )
											  return ret; }
          sep2
		| cdata VPB_TOKEN sep1 number
										{ if ( ( ret = set_vpb( $4 ) ) )
											  return ret; }
          sep2
		| cdata POL_TOKEN sep1 NUM_TOKEN
										{ if ( ( ret = set_bipolar( $4 ) ) )
											  return ret; }
          sep2
		| cdata EXTTRIG_TOKEN sep1 NUM_TOKEN
										{ if ( ( ret = set_exttrig( $4 ) ) )
											  return ret; }
          sep2
		| cdata IDEL_TOKEN sep1 number
										{ if ( ( ret = set_intr_delay( $4 ) ) )
											  return ret; }
          sep2
;

number:   NUM_TOKEN unit                 { $$ = $1 * $2; }
        | FLOAT_TOKEN unit               { $$ = $1 * $2; }
;

unit:     /* empty */                    { $$ = 1.0; }
        | UNIT_TOKEN                     { $$ = $1; }
;

sep0:     /* empty */
        | ';'
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


/*--------------------------------------------------*
 * Function for setting the name of the device file
 *--------------------------------------------------*/

static int set_dev_file( const char *name )
{
	extern char *rulbus_dev_file;


	if ( rulbus_dev_file != NULL )
	{
		if ( ! strcmp( rulbus_dev_file, name ) )
			return RULBUS_CF_DEV_FILE_DUPLICATE;
		else
			return RULBUS_OK;
	}

	if ( ( rulbus_dev_file = strdup( name ) ) == NULL )
		return RULBUS_NO_MEMORY;

	return RULBUS_OK;
}


/*----------------------------------------------*
 * Function called for rack address assignments
 *----------------------------------------------*/

static int rack_addr( int addr )
{
	int i;


	/* Check that no other address has been set for the rack yet */

	if ( rack == addr )
		return RULBUS_OK;

	if ( rack != RULBUS_INV_RACK_ADDR )
		return RULBUS_CF_RACK_ADDR_DUPLICATE;

	/* Check that the address is valid */

	if ( addr < 0 || addr > RULBUS_DEF_RACK_ADDR )
		return RULBUS_CF_RACK_ADDR_INVALID;

	/* Check that it isn't already used by a different rack */

	if ( rulbus_rack_addrs[ addr ] )
		return RULBUS_CF_RACK_ADDR_CONFLICT;

	/* Mark the address as used */

	rulbus_rack_addrs[ addr ] = SET;

	rack = addr;

	/* Set the rack address for all cards that haven't been assigned one */

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( rulbus_card[ i ].rack == RULBUS_INV_RACK_ADDR )
			rulbus_card[ i ].rack = ( unsigned char ) rack;

	return RULBUS_OK;
}


/*-------------------------------------------------*
 * Function called at the end of a rack definition
 *-------------------------------------------------*/

static int setup_cards( void )
{
	int i;


	/* Handle the situation where no rack address has been specified. In this
	   it gets assigned the default address of 0x0F (that also has that the
	   special property that then this is the only rack that can be
	   accessed and is always selected). */

	if ( rack == RULBUS_INV_RACK_ADDR )
	{
		if ( rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR ] )
			return RULBUS_CF_RACK_ADDR_CONFLICT;

		for ( i = 0; i < RULBUS_DEF_RACK_ADDR; i++ )
			if ( rulbus_rack_addrs[ i ] )
				return RULBUS_CF_RACK_ADDR_DEF_DUPLICATE;

		rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR ] = SET;

		rack = RULBUS_DEF_RACK_ADDR;

		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].rack == RULBUS_INV_RACK_ADDR )
				rulbus_card[ i ].rack = RULBUS_DEF_RACK_ADDR;
	}

	return RULBUS_OK;
}


/*------------------------------------------------*
 * Function to be called when a new card is found
 *------------------------------------------------*/

static int new_card( int type, char *name )
{
	RULBUS_CARD_LIST *tmp;
	int i;


	/* Weed out unsupported card types */

	if ( type == RB8506 || type == RB8506 || type == RB8506 ||
		 type == RB8513 || type == RB8905 || type == RB9005 ||
		 type == RB9603 )
		return RULBUS_CF_UNSUPPORTED_CARD_TYPE;

	/* Check that the name isn't already used for another card */

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( ! strcmp( name, rulbus_card[ i ].name ) )
			 return RULBUS_CF_CARD_NAME_CONFLICT;

	/* Set up a new structure for the card */

	if ( ( tmp = realloc( rulbus_card,
						  ( rulbus_num_cards + 1 ) * sizeof *tmp ) ) == NULL )
		return RULBUS_NO_MEMORY;

	rulbus_card = tmp;

	if ( ( rulbus_card[ rulbus_num_cards ].name = strdup( name ) ) == NULL )
		return RULBUS_NO_MEMORY;

	card = rulbus_card + rulbus_num_cards++;
	card->type = type;
	card->rack = ( unsigned char ) rack;

	/* Set all other card properties to invalid values so we we can tell where
	   we need to set default values later on */

	card->addr = RULBUS_INV_CARD_ADDR;
	card->num_channels = -1;
	card->vpb = -1.0;
	card->bipolar = -1;
	card->ext_trigger = -1;
	card->intr_delay = -1.0;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 *----------------------------------------------------------------*/

static int set_defaults( RULBUS_CARD_LIST *card )
{
	int i;


	/* Set default properties for card if necessray */

	switch ( card->type )
	{
		case RB8509 :
			if ( ( ret = set_rb8509_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB8510 :
			if ( ( ret = set_rb8510_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB8514 :
			if ( ( ret = set_rb8514_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB8515 :
			if ( ( ret = set_rb8515_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB_GENERIC :
			return set_rb_generic_defaults( card );
	}

	/* Check the cards address range does not overlap with the range of a
	   different card in the same rack (don't check cards of type
	   "rb_generic", they are supposed to cover the whole address range) */

	for ( i = 0; i < rulbus_num_cards; i++ )
	{
		if ( rulbus_card[ i ].type == RB_GENERIC ||
			 rulbus_card + i == card ||
			 rulbus_card[ i ].rack != card->rack )
			continue;

		if ( ( rulbus_card[ i ].addr < card->addr &&
			   rulbus_card[ i ].addr + rulbus_card[ i ].width > card->addr ) ||
			 ( card->addr < rulbus_card[ i ].addr &&
			   card->addr + card->width > rulbus_card[ i ].addr ) )
			return RULBUS_CF_CARD_ADDR_OVERLAP;
	}

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8509 cards
 *----------------------------------------------------------------*/

static int set_rb8509_defaults( RULBUS_CARD_LIST *card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless it hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].addr == RULBUS_RB8509_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8509_DEF_ADDR;
	}

	if ( card->num_channels == -1 )
		card->num_channels = RULBUS_RB8509_DEF_NUM_CHANNELS;

	if ( card->vpb <= 0.0 )
		card->vpb = RULBUS_RB8509_DEF_VPB;

	if ( card->bipolar == -1 )
		card->bipolar = RULBUS_RB8509_DEF_BIPOLAR;

	if ( card->ext_trigger == -1 )
		card->ext_trigger = RULBUS_RB8509_DEF_EXT_TRIGGER;

	card->width = RULBUS_RB8509_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8510 cards
 *----------------------------------------------------------------*/

static int set_rb8510_defaults( RULBUS_CARD_LIST *card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless it hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].addr == RULBUS_RB8510_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8510_DEF_ADDR;
	}

	if ( card->vpb <= 0.0 )
		card->vpb = RULBUS_RB8510_DEF_VPB;

	if ( card->bipolar == -1 )
		card->bipolar = RULBUS_RB8510_DEF_BIPOLAR;

	card->width = RULBUS_RB8510_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8514 cards
 *----------------------------------------------------------------*/

static int set_rb8514_defaults( RULBUS_CARD_LIST *card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless it hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].addr == RULBUS_RB8514_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8514_DEF_ADDR;
	}

	if ( card->intr_delay <= 0.0 )
		card->intr_delay = RULBUS_RB8514_DEF_INTR_DELAY;

	card->width = RULBUS_RB8510_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8515 cards
 *----------------------------------------------------------------*/

static int set_rb8515_defaults( RULBUS_CARD_LIST *card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless it hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].addr == RULBUS_RB8515_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8515_DEF_ADDR;
	}

	card->width = RULBUS_RB8510_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of generic cards
 *----------------------------------------------------------------*/

static int set_rb_generic_defaults( RULBUS_CARD_LIST *card )
{
	/* The address for the generic card is always 0 and the width is set
	   to the whole range plus one (which then allows access to all
	   possible addresses). */

	card->addr = 0;
	card->width = RULBUS_MAX_CARD_ADDR + 1;

	return RULBUS_OK;
}


/*--------------------------------------------------------------------*
 * Function that gets called when a card address is found in the file
 *--------------------------------------------------------------------*/

static int set_addr( int addr )
{
	int i;


	/* RB_GENERIC cards don't allow setting an address */

	if ( card->type == RB_GENERIC )
		return RULBUS_CF_CARD_ADDR_GENERIC;

	/* Check that the address is reasonable */

	if ( addr < RULBUS_MIN_CARD_ADDR || addr > RULBUS_MAX_CARD_ADDR )
		return RULBUS_CF_CARD_ADDR_INVALID;

	/* Check that the card hasn't already been assigned an address */

	if ( card->addr != RULBUS_INV_CARD_ADDR && card->addr != addr )
		return RULBUS_CF_CARD_ADDR_DUPLICATE;

	/* Check that the address hasn't been used for a different card in the
	   same rack (the rack addresses of all cards of the rack are still set
	   to an invalid value) */

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( rulbus_card[ i ].rack == RULBUS_INV_RACK_ADDR &&
			 rulbus_card[ i ].addr == addr )
			return RULBUS_CF_CARD_ADDR_CONFLICT;;

	card->addr = ( unsigned char ) addr;
	return RULBUS_OK;
}


/*---------------------------------------------------------------------------*
 * Function to be called when a 'num_channels' property is found in the file
 *---------------------------------------------------------------------------*/

static int set_nchan( int nchan )
{
	/* Check if card has this property */

	if ( card->type != RB8509 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a number of channels */

	if ( card->num_channels != -1 && card->num_channels != nchan )
		return RULBUS_CF_DUPLICATE_NUM_CHANNELS;

	/* Check that it's not too large */

	if ( nchan < 1 || nchan > RULBUS_RB8509_MAX_CHANNELS )
		return RULBUS_CF_INVALID_NUM_CHANNELS;

	card->num_channels = nchan;
	return RULBUS_OK;
}

/*---------------------------------------------------------------------------*
 * Function to be called when a 'volt_per_bit' property is found in the file
 *---------------------------------------------------------------------------*/

static int set_vpb( double vpb )
{
	/* Check if card has this property */

	if ( card->type != RB8509 && card->type != RB8510 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a volt_per_bit value */

	if ( card->vpb >= 0.0 && card->vpb != vpb )
		return RULBUS_CF_VPB_DUPLICATE;

	/* Check that the argument isn't completely unreasonable */

	if ( vpb <= 0.0 )
		return RULBUS_CF_INVALID_VPB;

	card->vpb = vpb;
	return RULBUS_OK;
}


/*----------------------------------------------------------------------*
 * Function to be called when a 'bipolar' property is found in the file
 *----------------------------------------------------------------------*/

static int set_bipolar( int is_bipolar )
{
	/* Check if card has this property */

	if ( card->type != RB8509 && card->type != RB8510 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a polarity */

	if ( card->bipolar != -1 && card->bipolar != ( is_bipolar == 1 ) )
		return RULBUS_CF_BIPLOAR_DUPLICATE;

	card->bipolar = is_bipolar == 1;

	return RULBUS_OK;
}


/*----------------------------------------------------------------------*
 * Function to be called when a 'bipolar' property is found in the file
 *----------------------------------------------------------------------*/

static int set_exttrig( int is_ext_trigger )
{
	/* Check if card has this property */

	if ( card->type != RB8509 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a polarity */

	if ( card->ext_trigger != -1 &&
		 card->ext_trigger != ( is_ext_trigger == 1 ) )
		return RULBUS_CF_EXT_TRIGGER_DUPLICATE;

	card->ext_trigger = is_ext_trigger == 1;

	return RULBUS_OK;
}


/*---------------------------------------------.---------------------------*
 * Function to be called when a 'intr_delay' property is found in the file
 *-------------------------------------------------------------------------*/

static int set_intr_delay( double intr_delay )
{
	/* Check if card has this property */

	if ( card->type != RB8514 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a type */

	if ( card->intr_delay > 0.0 && card->intr_delay != intr_delay )
		return RULBUS_CF_INTR_DELAY_DUPLICATE;

	/* Check that the argument isn't completely unreasonable */

	if ( intr_delay < 0.0 )
		return RULBUS_CF_INTR_DELAY_INVALID;

	card->intr_delay = intr_delay;
	return RULBUS_OK;
}


/*-----------------------------------------------------*
 * Dummy function (required by bison) called on errors
 *-----------------------------------------------------*/

static void rulbus_error( const char *s )
{
	s = s;
}


/*-----------------------------------------------------*
 * Function for initialization of the parser and lexer
 *-----------------------------------------------------*/

void rulbus_parser_init( void )
{
	int i;
	extern int rulbus_column;


	rulbus_column = 0;

	for ( i = 0; i <= RULBUS_DEF_RACK_ADDR; i++ )
		rulbus_rack_addrs[ i ] = UNSET;
}
