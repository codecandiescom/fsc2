/*
 *  $Id$
 *
 *  Copyright (C) 2003-2004 Jens Thoms Toerring
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
 *  To contact the author send email to
 *  Jens.Toerring@physik.fu-berlin.de
 */


#include "rulbus_lib.h"


typedef struct RULBUS_RB8510_DAC12_CARD RULBUS_RB8510_DAC12_CARD;

struct RULBUS_RB8510_DAC12_CARD {
	int handle;
	unsigned short int v;
	double dV;
	double Vmin;
	double Vmax;
};


/* Upper limit voltages the DACs can be configured to */

#define DAC12_RANGE  0x0FFF

#define DAC12_MSB    0
#define DAC12_LSB    1


static RULBUS_RB8510_DAC12_CARD *rulbus_rb8510_dac12_card = NULL;
static int rulbus_num_dac12_cards = 0;


static RULBUS_RB8510_DAC12_CARD *rulbus_rb8510_dac12_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the dac12 card subsystem (gets invoked
 * automatically by the rulbus_open() function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_rb8510_dac12_init( void )
{
	rulbus_rb8510_dac12_card = NULL;
	rulbus_num_dac12_cards = 0;
	return rulbus_errno = RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the dac12 card subsystem (gets invoked
 * automatically by the rulbus_close() function, so it's not to be
 * called by the user directly). Usually it won't even do anything,
 * at least if rulbus_rb8510_dac12_card_exit() has been called for
 * all existing cards.
 *------------------------------------------------------------------*/

void rulbus_rb8510_dac12_exit( void )
{
	if ( rulbus_rb8510_dac12_card == NULL )
		return;

	free( rulbus_rb8510_dac12_card );
	rulbus_rb8510_dac12_card = NULL;
	rulbus_num_dac12_cards = 0;
}


/*---------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open() function, so it's not to be called by the
 * user directly)
 *---------------------------------------------------------------------*/

int rulbus_rb8510_dac12_card_init( int handle )
{
	RULBUS_RB8510_DAC12_CARD *tmp;


	tmp = realloc( rulbus_rb8510_dac12_card,
				   ( rulbus_num_dac12_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return rulbus_errno = RULBUS_NO_MEMORY;

	rulbus_rb8510_dac12_card = tmp;
	tmp += rulbus_num_dac12_cards++;

	tmp->handle = handle;
	tmp->dV = rulbus_card[ handle ].vpb;

	if ( rulbus_card[ handle ].bipolar )
	{
		tmp->Vmax = tmp->dV * ( DAC12_RANGE >> 1 );
		tmp->Vmin = tmp->Vmax - tmp->dV * DAC12_RANGE;
	}
	else
	{
		tmp->Vmax = tmp->dV * DAC12_RANGE;
		tmp->Vmin = 0.0;
	}

	tmp->v = DAC12_RANGE + 1;                     /* impossible value */

	return rulbus_errno = RULBUS_OK;
}
	

/*---------------------------------------------------------------*
 * Function for deactivating a card (gets invoked automatically
 * by the rulbus_card_close() function, so it's not to be called
 * by the user directly)
 *---------------------------------------------------------------*/

int rulbus_rb8510_dac12_card_exit( int handle )
{
	RULBUS_RB8510_DAC12_CARD *card;


	/* Try to find the card, if it doesn't exist just return */

	if ( ( card = rulbus_rb8510_dac12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_OK;

	/* Remove the entry for the card */

	if ( rulbus_num_dac12_cards > 1 )
	{
		if ( card != rulbus_rb8510_dac12_card + rulbus_num_dac12_cards - 1 )
			memmove( card, card + 1, sizeof *card *
					 ( rulbus_num_dac12_cards -
					   ( card - rulbus_rb8510_dac12_card ) - 1 ) );

		card = realloc( rulbus_rb8510_dac12_card,
						( rulbus_num_dac12_cards - 1 ) * sizeof *card );

		if ( card == NULL )
			return rulbus_errno = RULBUS_NO_MEMORY;

		rulbus_rb8510_dac12_card = card;
	}
	else
	{
		free( rulbus_rb8510_dac12_card );
		rulbus_rb8510_dac12_card = NULL;
	}

	rulbus_num_dac12_cards--;

	return rulbus_errno = RULBUS_OK;
}


/*--------------------------------------------------*
 * Function for enquiring about the DACs properties 
 *--------------------------------------------------*/

int rulbus_rb8510_dac12_properties( int handle, double *Vmax, double *Vmin,
									double *dV )
{
	RULBUS_RB8510_DAC12_CARD *card;


	if ( ( card = rulbus_rb8510_dac12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( Vmax )
		*Vmax = card->Vmax;

	if ( Vmin )
		*Vmin = card->Vmin;

	if ( dV )
		*dV = card->dV;

	return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------*
 * Function for setting a new output voltage
 *-------------------------------------------*/

int rulbus_rb8510_dac12_set_voltage( int handle, double volts )
{
	RULBUS_RB8510_DAC12_CARD *card;
	int retval;
	unsigned char byte;
	unsigned short int val;


	if ( ( card = rulbus_rb8510_dac12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( volts + 0.5 * card->dV < card->Vmin ||
		 volts - 0.5 * card->dV > card->Vmax )
		return rulbus_errno = RULBUS_INVALID_VOLTAGE;

	val = ( unsigned short int )
							  floor( ( volts - card->Vmin ) / card->dV + 0.5 );

	if ( card->v == val )
		return rulbus_errno = RULBUS_OK;

	card->v = val;

	byte = ( val >> 8 ) & 0xFF;
	if ( ( retval = rulbus_write( handle, DAC12_MSB, &byte, 1 ) ) != 1 )
		return rulbus_errno = retval;

	byte = val & 0xFF;
	if ( ( retval = rulbus_write( handle, DAC12_LSB, &byte, 1 ) ) != 1 )
		return rulbus_errno = retval;

	return rulbus_errno = RULBUS_OK;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_RB8510_DAC12_CARD *rulbus_rb8510_dac12_card_find( int handle )
{
	int i;


	if ( handle < 0 )
		return NULL;

	for ( i = 0; i < rulbus_num_dac12_cards; i++ )
		if ( handle == rulbus_rb8510_dac12_card[ i ].handle )
			return rulbus_rb8510_dac12_card + i;

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
