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


#include "rulbus_lib.h"


typedef struct RULBUS_DELAY_CARD RULBUS_DELAY_CARD;

struct RULBUS_DELAY_CARD {
	int handle;
	unsigned char ctrl;
	unsigned long delay;
};


#define STATUS_ADDR   3
#define CONTROL_ADDR  3


/* Bits in the control byte */

#define START_PULSE_OUT_1_ENABLE  ( 1 << 0 )
#define END_PULSE_OUT_1_ENABLE    ( 1 << 1 )
#define START_PULSE_OUT_2_ENABLE  ( 1 << 2 )
#define END_PULSE_OUT_2_ENABLE    ( 1 << 3 )
#define INTERRUPT_ENABLE          ( 1 << 4 )
#define TRIGGER_ON_FALLING_EDGE   ( 1 << 5 )
#define START_PULSE_POSITIVE      ( 1 << 6 )
#define END_PULSE_POSITIVE        ( 1 << 7 )

/* Bits in the status byte */

#define DELAY_BUSY                ( 1 << 1 )

#define INVALID_DELAY             ( RULBUS_DELAY_CARD_MAX + 1 )


static RULBUS_DELAY_CARD *rulbus_delay_card;
static int rulbus_num_delay_cards;


static RULBUS_DELAY_CARD *rulbus_delay_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the delay card subsystem (gets invoked
 * automatically by the rulbus_open( ) function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_delay_init( void )
{
	rulbus_delay_card = NULL;
	rulbus_num_delay_cards = 0;
	return RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the delay card subsystem (gets invoked
 * automatically by the rulbus_close( ) function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

void rulbus_delay_exit( void )
{
	if ( rulbus_delay_card == NULL )
		return;

	free( rulbus_delay_card );
	rulbus_delay_card = NULL;
	rulbus_num_delay_cards = 0;
}


/*----------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open( ) function, so it's not to be called by the
 * user directly)
 *----------------------------------------------------------------------*/

int rulbus_delay_card_init( int handle )
{
	RULBUS_DELAY_CARD *tmp;


	tmp = realloc( rulbus_delay_card,
				   ( rulbus_num_delay_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return RULBUS_NO_MEM;

	rulbus_delay_card = tmp;
	tmp += rulbus_num_delay_cards++;

	tmp->handle = handle;
	tmp->delay = INVALID_DELAY;
	tmp->ctrl = END_PULSE_OUT_1_ENABLE | END_PULSE_POSITIVE;

	/* Set a few defaults: output a (positive) end pulse, start pulse on a
	   raising trigger edge and disable interrupts */

	return rulbus_write( handle, CONTROL_ADDR, &tmp->ctrl, 1 );
}
	

/*----------------------------------------------------------------*
 * Function for deactivation a card (gets invoked automatically
 * by the rulbus_card_close( ) function, so it's not to be called
 * by the user directly)
 *----------------------------------------------------------------*/

void rulbus_delay_card_exit( int handle )
{
	RULBUS_DELAY_CARD *card;


	/* Try to find the card, if it doesn't exist just return */

	if ( ( card = rulbus_delay_card_find( handle ) ) == NULL )
		return;

	/* Remove the entry for the card */

	if ( card != rulbus_delay_card + rulbus_num_delay_cards - 1 )
		memcpy( card, card + 1, sizeof *card *
				rulbus_num_delay_cards - ( card - rulbus_delay_card ) - 1 );

	card = realloc( rulbus_delay_card,
					( rulbus_num_delay_cards - 1 ) * sizeof *card );

	if ( card != NULL )
		rulbus_delay_card = card;

	rulbus_num_delay_cards--;
}


/*-----------------------------------------*
 * Function for setting the delay duration
 *-----------------------------------------*/

int rulbus_delay_set_delay( int handle, unsigned long delay )
{
	RULBUS_DELAY_CARD *card;
	unsigned char byte;
	unsigned char i;
	int retval;


	if ( ( card = rulbus_delay_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( delay > RULBUS_DELAY_CARD_MAX )
		return RULBUS_INV_ARG;

	if ( delay == card->delay )
		return RULBUS_OK;

	if ( ( retval = rulbus_read( handle, STATUS_ADDR, &byte, 1 ) ) < 0 )
		 return retval;

	if ( byte & DELAY_BUSY )
		return RULBUS_CRD_BSY;

	for ( i = 0; i < 3; delay >>= 8, i++ )
	{
		byte = ( unsigned char ) ( delay & 0xFF );
		if ( ( retval = rulbus_write( handle, 2 - i, &byte, 1 ) ) < 0 )
			 return retval;
	}

	card->delay = delay;
	return RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for selecting if the delay is started by the falling or
 * the raising edge of the external trigger
 *------------------------------------------------------------------*/

int rulbus_delay_set_trigger( int handle, int edge )
{
	RULBUS_DELAY_CARD *card;
	unsigned char ctrl;


	if ( ( card = rulbus_delay_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( edge != RULBUS_DELAY_FALLING_EDGE &&
		 edge != RULBUS_DELAY_RAISING_EDGE )
		return RULBUS_INV_ARG;

	ctrl = card->ctrl;

	if ( edge == RULBUS_DELAY_FALLING_EDGE )
		ctrl |= TRIGGER_ON_FALLING_EDGE;
	else
		ctrl &= ~ TRIGGER_ON_FALLING_EDGE;

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	return rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 );
}


/*-------------------------------------------------------------*
 * Function for selecting if a start and/or end pulse is to be
 * output at output 1 and/or 2
 *-------------------------------------------------------------*/

int rulbus_delay_set_output_pulse( int handle, int output, int type )
{
	RULBUS_DELAY_CARD *card;
	unsigned char ctrl;


	if ( ( card = rulbus_delay_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( ( output != RULBUS_DELAY_OUTPUT_1 &&
		   output != RULBUS_DELAY_OUTPUT_2 &&
		   output != RULBUS_DELAY_OUTPUT_BOTH ) ||
		 ( type != RULBUS_DELAY_START_PULSE &&
		   type != RULBUS_DELAY_END_PULSE &&
		   type != RULBUS_DELAY_PULSE_BOTH ) )
		return RULBUS_INV_ARG;

	ctrl = card->ctrl & 0xF0;

	if ( type & RULBUS_DELAY_OUTPUT_1 )
	{
		if ( output & RULBUS_DELAY_START_PULSE )
			ctrl |= START_PULSE_OUT_1_ENABLE;
		if ( output & RULBUS_DELAY_END_PULSE )
			ctrl |= END_PULSE_OUT_1_ENABLE;
	}
		
	if ( type & RULBUS_DELAY_OUTPUT_2 )
	{
		if ( output & RULBUS_DELAY_START_PULSE )
			ctrl |= START_PULSE_OUT_2_ENABLE;
		if ( output & RULBUS_DELAY_END_PULSE )
			ctrl |= END_PULSE_OUT_2_ENABLE;
	}

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	return rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 );
}


/*-------------------------------------------------------------*
 * Function for selecting if a start and/or end pulse is to be
 * output at output 1 and/or 2
 *-------------------------------------------------------------*/

int rulbus_delay_set_output_pulse_polarity( int handle, int type, int pol )
{
	RULBUS_DELAY_CARD *card;
	unsigned char ctrl;


	if ( ( card = rulbus_delay_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( ( type != RULBUS_DELAY_START_PULSE &&
		   type != RULBUS_DELAY_END_PULSE &&
		   type != RULBUS_DELAY_PULSE_BOTH ) ||
		 ( pol != RULBUS_DELAY_POLARITY_NEGATIVE &&
		   pol != RULBUS_DELAY_POLARITY_POSITIVE ) )
		return RULBUS_INV_HND;

	ctrl = card->ctrl;

	if ( type & RULBUS_DELAY_START_PULSE )
	{
		if ( pol == RULBUS_DELAY_POLARITY_NEGATIVE )
			ctrl &= ~ START_PULSE_POSITIVE;
		else
			ctrl |= START_PULSE_POSITIVE;
	}

	if ( type & RULBUS_DELAY_END_PULSE )
	{
		if ( pol == RULBUS_DELAY_POLARITY_NEGATIVE )
			ctrl &= ~ END_PULSE_POSITIVE;
		else
			ctrl |= END_PULSE_POSITIVE;
	}

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	return rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 );
}


/*---------------------------------------------------------------*
 * According to the manual a delay can be started by setting the
 * trigger first to falling and the immediately to raising edge
 *---------------------------------------------------------------*/

int rulbus_delay_software_start( int handle )
{
	RULBUS_DELAY_CARD *card;
	unsigned char ctrl;
	int retval;


	if ( ( card = rulbus_delay_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	ctrl = card->ctrl;

	ctrl |= TRIGGER_ON_FALLING_EDGE;
	if ( ( retval = rulbus_write( handle, CONTROL_ADDR, &ctrl, 1 ) ) < 0 )
		return retval;
		 
	ctrl &= ~ TRIGGER_ON_FALLING_EDGE;
	if ( ( retval = rulbus_write( handle, CONTROL_ADDR, &ctrl, 1 ) ) < 0 )
		return retval;

	if ( ctrl != card->ctrl &&
		 ( retval = rulbus_write( handle, CONTROL_ADDR, &ctrl, 1 ) ) < 0 )
		return retval;

	return RULBUS_OK;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_DELAY_CARD *rulbus_delay_card_find( int handle )
{
	int i;


	if ( handle < 0 )
		return NULL;

	for ( i = 0; i < rulbus_num_delay_cards; i++ )
		if ( handle == rulbus_delay_card[ i ].handle )
			return rulbus_delay_card + i;

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
