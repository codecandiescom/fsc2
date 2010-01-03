/*
 *  Copyright (C) 2003-2010 Jens Thoms Toerring
 *
 *  Library for Rulbus (Rijksuniversiteit Leiden BUS)
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  The library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the package; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 *  To contact the author send email to jt@toerring.de
 */


%{

#include "rulbus_lib.h"
#include <limits.h>

extern int rulbus_lex( void );

void rulbus_parser_init( void );

void rulbus_parser_clean_up( void );

extern RULBUS_CARD_LIST *rulbus_card;
extern int rulbus_num_cards;

static int rack;                 /* address of current rack */
static RULBUS_CARD_LIST *card;   /* pointer to current card */
static int ret;                  
static int rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR + 1 ];
static char **rulbus_rack_names = NULL;
static int rulbus_num_racks = 0;

static int set_dev_file( const char * name );

static int rack_addr( int addr );

static int rack_name( const char * name );

static int setup_rack( void );

static int finalize_rack( void );

static int new_card( int          type,
					 const char * name );

static int set_defaults( RULBUS_CARD_LIST * card );

static int set_rb8509_defaults( RULBUS_CARD_LIST * card );

static int set_rb8510_defaults( RULBUS_CARD_LIST * card );

static int set_rb8514_defaults( RULBUS_CARD_LIST * card );

static int set_rb8515_defaults( RULBUS_CARD_LIST * card );

static int set_rb_generic_defaults( RULBUS_CARD_LIST * card );

static int set_addr( int addr );

static int set_nchan( int nchan );

static int set_vpb( double vpb );

static int set_bipolar( int is_bipolar );

static int set_exttrig( int is_ext_trigger );

static int set_intr_delay( double intr_delay );

static void rulbus_error( const char * s );

static inline long lrnd( double x );

%}

%union
{
    int ival;
	double dval;
    char *sval;
}

%token FILE_TOKEN RACK_TOKEN ADDR_TOKEN VPB_TOKEN POL_TOKEN NUM_TOKEN
%token CARD_TOKEN STR_TOKEN NCHAN_TOKEN ERR_TOKEN EOFC_TOKEN EXTTRIG_TOKEN
%token FLOAT_TOKEN UNIT_TOKEN IDEL_TOKEN

%type <ival> NUM_TOKEN CARD_TOKEN
%type <dval> FLOAT_TOKEN UNIT_TOKEN number unit
%type <sval> STR_TOKEN rname;


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

rack:     RACK_TOKEN     		   		{ rack = RULBUS_INV_RACK_ADDR; }
		  rname                         { if ( ( ret = rack_name( $3 ) ) )
			                                  return ret; }
		  sep1 rinit sep2
;

rname:    /* empty */                   { return RULBUS_CF_NO_RACK_NAME; }
		| STR_TOKEN                     { $$ = $1; }
;

rinit:    /* empty */
		| '{' rdata '}'                 { if ( ( ret = finalize_rack( ) ) )
			                                  return ret; }
;

rdata:    /* empty */
        | rdata ADDR_TOKEN sep1 NUM_TOKEN
	                                    { if ( ( ret = rack_addr( $4 ) ) )
											  return ret; }
          sep2
		| rdata CARD_TOKEN STR_TOKEN    { if ( ( ret = new_card( $2, $3 ) ) )
											  return ret; }
		  sep1 cinit sep2
;

cinit:    /* empty */                   { if ( ( ret = set_defaults( card ) ) )
			                                  return ret; }
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

static int set_dev_file( const char * name )
{
	extern char *rulbus_dev_file;


	if ( rulbus_dev_file != NULL )
	{
		if ( strcmp( rulbus_dev_file, name ) )
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
	/* Check that no other address has been set for the rack yet */

	if ( rack == addr )
		return RULBUS_OK;

	if ( rack != RULBUS_INV_RACK_ADDR )
		return RULBUS_CF_RACK_ADDR_DUPLICATE;

	/* Check that the address is valid */

	if ( addr < 0 || addr > RULBUS_DEF_RACK_ADDR )
		return RULBUS_CF_RACK_ADDR_INVALID;

	rack = addr;

	return RULBUS_OK;
}


/*-------------------------------------------*
 * Function called for rack name assignments
 *-------------------------------------------*/

static int rack_name( const char * name )
{
	char **rns;
	int i;


	for ( i = 0; i < rulbus_num_racks; i++ )
		if ( ! strcmp( name, rulbus_rack_names[ i ] ) )
			return RULBUS_CF_RACK_NAME_CONFLICT;

	if ( ( rns = realloc( rulbus_rack_names,
						  ( rulbus_num_racks + 1 ) * sizeof *rns ) ) == NULL )
		return RULBUS_NO_MEMORY;

	rulbus_rack_names = rns;

	if ( ( rulbus_rack_names[ rulbus_num_racks++ ] = strdup( name ) ) == NULL )
		return RULBUS_NO_MEMORY;

	return RULBUS_OK;
}


/*----------------------------------------*
 * Function to set a default rack address 
 *----------------------------------------*/

static int setup_rack( void )
{
	int i;


	if ( rack != RULBUS_INV_RACK_ADDR )
	{
		if ( rulbus_rack_addrs[ rack ] )
			return RULBUS_CF_RACK_ADDR_CONFLICT;

		if ( rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR ] )
			return RULBUS_CF_RACK_ADDR_DEF_DUPLICATE;

		rulbus_rack_addrs[ rack ] = SET;
		return RULBUS_OK;
	}

	/* Handle the situation where no rack address has been specified. In this
	   case assign it the default address of 0x0F (that also has the special
	   property that then this is the only rack that can be accessed and is
	   always selected). */

	if ( rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR ] )
		return RULBUS_CF_RACK_ADDR_DEF_DUPLICATE;

	for ( i = 0; i < RULBUS_DEF_RACK_ADDR; i++ )
		if ( rulbus_rack_addrs[ i ] )
			return RULBUS_CF_RACK_ADDR_DEF_DUPLICATE;

	rulbus_rack_addrs[ RULBUS_DEF_RACK_ADDR ] = SET;

	rack = RULBUS_DEF_RACK_ADDR;

	return RULBUS_OK;
}


/*---------------------------------------------------------------------*
 * Function called at the end of a rack definition that contains cards
 *---------------------------------------------------------------------*/

static int finalize_rack( void )
{
	int retval;
	int i = 0;


	/* No further checks needed if there are no cards without a valid rack
	   address (i.e there are no cards in the rack we're dealing with) */

	if ( rulbus_num_cards == 0 ||
		 rulbus_card[ rulbus_num_cards - 1 ].rack != RULBUS_INV_RACK_ADDR )
		return RULBUS_OK;

	/* Check the rack address */
	
	if ( ( retval = setup_rack( ) ) != RULBUS_OK )
		return retval;

	for ( i = rulbus_num_cards - 1;
		  rulbus_card[ i ].rack == RULBUS_INV_RACK_ADDR && i >= 0; --i )
		rulbus_card[ i ].rack = rack;

	return RULBUS_OK;
}


/*------------------------------------------------*
 * Function to be called when a new card is found
 *------------------------------------------------*/

static int new_card( int          type,
					 const char * name )
{
	RULBUS_CARD_LIST *tmp;
	int i;


	/* Weed out unsupported card types */

	if ( type == RB8506_PIA   || type == RB8506_SIFU      ||
		 type == RB8506_VIA   || type == RB8513_TIMEBASE  ||
		 type == RB8905_ADC12 || type == RB9005_AMPLIFIER ||
		 type == RB9603_MONOCHROMATOR )
		return RULBUS_CF_UNSUPPORTED_CARD_TYPE;

	/* Check that the name isn't already used for another card */

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( ! strcmp( name, rulbus_card[ i ].name ) )
			 return RULBUS_CF_CARD_NAME_CONFLICT;

	/* Append a new structure for the card to the list */

	if ( ( tmp = realloc( rulbus_card,
						  ( rulbus_num_cards + 1 ) * sizeof *tmp ) ) == NULL )
		return RULBUS_NO_MEMORY;

	rulbus_card = tmp;

	if ( ( rulbus_card[ rulbus_num_cards ].name = strdup( name ) ) == NULL )
		return RULBUS_NO_MEMORY;

	card = rulbus_card + rulbus_num_cards++;
	card->type = type;

	/* Set all other card properties to invalid values so we we can tell where
	   we need to set default values later on */

	card->rack = RULBUS_INV_RACK_ADDR;
	card->addr = RULBUS_INV_CARD_ADDR;
	card->num_channels = -1;
	card->vpb = -1.0;
	card->bipolar = -1;
	card->has_ext_trigger = -1;
	card->intr_delay = -1.0;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 *----------------------------------------------------------------*/

static int set_defaults( RULBUS_CARD_LIST * card )
{
	int i;


	/* Set default properties for card if necessray */

	switch ( card->type )
	{
		case RB8509_ADC12 :
			if ( ( ret = set_rb8509_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB8510_DAC12 :
			if ( ( ret = set_rb8510_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB8514_DELAY :
			if ( ( ret = set_rb8514_defaults( card ) ) != RULBUS_OK )
				return ret;
			break;

		case RB8515_CLOCK :
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
		if ( rulbus_card + i == card ||
			 rulbus_card[ i ].rack != card->rack ||
			 rulbus_card[ i ].type == RB_GENERIC )
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
 * of RB8509 ADC12 cards
 *----------------------------------------------------------------*/

static int set_rb8509_defaults( RULBUS_CARD_LIST * card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless that one hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].rack == card->rack &&
				 rulbus_card[ i ].addr == RULBUS_RB8509_ADC12_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8509_ADC12_DEF_ADDR;
	}

	if ( card->num_channels == -1 )
		card->num_channels = RULBUS_RB8509_ADC12_DEF_NUM_CHANNELS;

	if ( card->vpb <= 0.0 )
		card->vpb = RULBUS_RB8509_ADC12_DEF_VPB;

	if ( card->bipolar == -1 )
		card->bipolar = RULBUS_RB8509_ADC12_DEF_BIPOLAR;

	if ( card->has_ext_trigger == -1 )
		card->has_ext_trigger = RULBUS_RB8509_ADC12_DEF_EXT_TRIGGER;

	card->width = RULBUS_RB8509_ADC12_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8510 DAC12 cards
 *----------------------------------------------------------------*/

static int set_rb8510_defaults( RULBUS_CARD_LIST * card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless that one hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].rack == card->rack &&
				 rulbus_card[ i ].addr == RULBUS_RB8510_DAC12_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8510_DAC12_DEF_ADDR;
	}

	if ( card->vpb <= 0.0 )
		card->vpb = RULBUS_RB8510_DAC12_DEF_VPB;

	if ( card->bipolar == -1 )
		card->bipolar = RULBUS_RB8510_DAC12_DEF_BIPOLAR;

	card->width = RULBUS_RB8510_DAC12_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8514 delay cards
 *----------------------------------------------------------------*/

static int set_rb8514_defaults( RULBUS_CARD_LIST * card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless that hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].rack == card->rack &&
				 rulbus_card[ i ].addr == RULBUS_RB8514_DELAY_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8514_DELAY_DEF_ADDR;
	}

	if ( card->intr_delay < 0 )
		card->intr_delay = RULBUS_RB8514_DELAY_DEF_INTR_DELAY;

	card->width = RULBUS_RB8514_DELAY_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of RB8515 clock cards
 *----------------------------------------------------------------*/

static int set_rb8515_defaults( RULBUS_CARD_LIST * card )
{
	int i;


	/* If the card hasn't been assigned an address take the default address
	   (unless that one hasn't been assigned to another card) */

	if ( card->addr == RULBUS_INV_CARD_ADDR )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].rack == card->rack &&
				 rulbus_card[ i ].addr == RULBUS_RB8515_CLOCK_DEF_ADDR )
				return RULBUS_CF_CARD_ADDR_DEF_CONFLICT;

		card->addr = RULBUS_RB8515_CLOCK_DEF_ADDR;
	}

	card->width = RULBUS_RB8515_CLOCK_WIDTH;

	return RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting default values for unspecified properties
 * of generic cards
 *----------------------------------------------------------------*/

static int set_rb_generic_defaults( RULBUS_CARD_LIST * card )
{
	/* The address for the generic card is always 0 and the width is set
	   to the whole range plus one (which then allows access to all
	   possible addresses). */

	card->addr = RULBUS_RB_GENERIC_DEF_ADDR;
	card->width = RULBUS_MAX_CARD_ADDR + 1;

	return RULBUS_OK;
}


/*--------------------------------------------------------------------*
 * Function that gets called when a card address is found in the file
 *--------------------------------------------------------------------*/

static int set_addr( int addr )
{
	int i;


	/* RB_GENERIC cards don't allow setting an address other than 0 (which
	   is an invalid address for all other cards) */

	if ( card->type == RB_GENERIC )
	{
		if ( addr != 0 )
			return RULBUS_CF_CARD_ADDR_GENERIC;
		else
		{
			card->addr = 0;
			return RULBUS_OK;
		}
	}

	/* Check that the address is reasonable */

	if ( addr < RULBUS_MIN_CARD_ADDR || addr > RULBUS_MAX_CARD_ADDR )
		return RULBUS_CF_CARD_ADDR_INVALID;

	/* Check that the card hasn't already been assigned a different address */

	if ( card->addr != RULBUS_INV_CARD_ADDR && card->addr != addr )
		return RULBUS_CF_CARD_ADDR_DUPLICATE;

	/* Check that the address hasn't been used for a different card in the
	   same rack (the rack addresses of all cards of the rack are still set
	   to an invalid value) */

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( card != rulbus_card + i &&
			 rulbus_card[ i ].rack == RULBUS_INV_RACK_ADDR &&
			 rulbus_card[ i ].addr == addr )
			return RULBUS_CF_CARD_ADDR_CONFLICT;

	card->addr = ( unsigned char ) addr;
	return RULBUS_OK;
}


/*---------------------------------------------------------------------------*
 * Function to be called when a 'num_channels' property is found in the file
 *---------------------------------------------------------------------------*/

static int set_nchan( int nchan )
{
	/* Check if card has this property */

	if ( card->type != RB8509_ADC12 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a number of channels */

	if ( card->num_channels != -1 && card->num_channels != nchan )
		return RULBUS_CF_DUPLICATE_NUM_CHANNELS;

	/* Check that it's not too large */

	if ( nchan < 1 || nchan > RULBUS_RB8509_ADC12_MAX_CHANNELS )
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

	if ( card->type != RB8509_ADC12 && card->type != RB8510_DAC12 )
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

	if ( card->type != RB8509_ADC12 && card->type != RB8510_DAC12 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a polarity */

	if ( card->bipolar != -1 && card->bipolar != ( is_bipolar != 0 ) )
		return RULBUS_CF_BIPLOAR_DUPLICATE;

	card->bipolar = is_bipolar != 0;

	return RULBUS_OK;
}


/*----------------------------------------------------------------------*
 * Function to be called when a 'bipolar' property is found in the file
 *----------------------------------------------------------------------*/

static int set_exttrig( int has_ext_trigger )
{
	/* Check if card has this property */

	if ( card->type != RB8509_ADC12 )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a polarity */

	if ( card->has_ext_trigger != -1 &&
		 card->has_ext_trigger != ( has_ext_trigger != 0 ) )
		return RULBUS_CF_EXT_TRIGGER_DUPLICATE;

	card->has_ext_trigger = has_ext_trigger != 0;

	return RULBUS_OK;
}


/*---------------------------------------------.---------------------------*
 * Function to be called when a 'intr_delay' property is found in the file
 *-------------------------------------------------------------------------*/

static int set_intr_delay( double intr_delay )
{
	/* Check if card has this property */

	if ( card->type != RB8514_DELAY )
		return RULBUS_CF_CARD_PROPERTY_INVALID;

	/* Check that the card hasn't already been assigned a delay that differs
	   by more than a ns */

	if ( card->intr_delay >= 0.0 &&
		 lrnd( card->intr_delay * 1e9) != lrnd( intr_delay * 1e9 ) )
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

static void rulbus_error( const char * s )
{
	s = s;     /* keeps the compiler from complaining... */
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


/*----------------------------------------------------------*
 * Function that must be called when the parser is finished
 *----------------------------------------------------------*/

void rulbus_parser_clean_up( void )
{
	int i;


	if ( rulbus_rack_names != NULL )
	{
		for ( i = 0; i < rulbus_num_racks; i++ )
			if ( rulbus_rack_names[ i ] != NULL )
				free( rulbus_rack_names[ i ] );
		free( rulbus_rack_names );
	}

	rulbus_num_racks = 0;
	rulbus_rack_names = NULL;
}


/*-----------------------------------------*
 * Utility function: rounds double to long
 *-----------------------------------------*/

static inline long lrnd( double x )
{
	if ( x > LONG_MAX )
		return LONG_MAX;
	if ( x < LONG_MIN )
		return LONG_MIN;

	return ( long ) ( x < 0.0 ? ceil( x - 0.5 ) : floor( x + 0.5 ) );
}
