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


typedef struct RULBUS_DAC12_CARD RULBUS_DAC12_CARD;

struct RULBUS_DAC12_CARD {
	int handle;
	unsigned short int v;
	double dV;
	double Vmin;
	double Vmax;
};


/* Upper limit voltages the DACs can configured to */

static double ranges[ ] = { 2000, 1000, 500, 2048, 1024, 512 };

#define DAC12_RANGE  0x0FFF

#define DAC12_MSB    0
#define DAC12_LSB    1


static RULBUS_DAC12_CARD *rulbus_dac12_card = NULL;
static int rulbus_num_dac12_cards = 0;


static RULBUS_DAC12_CARD *rulbus_dac12_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the dac12 card subsystem (gets invoked
 * automatically by the rulbus_open( ) function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_dac12_init( void )
{
	rulbus_dac12_card = NULL;
	rulbus_num_dac12_cards = 0;
	return RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the dac12 card subsystem (gets invoked
 * automatically by the rulbus_close( ) function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

void rulbus_dac12_exit( void )
{
	if ( rulbus_dac12_card == NULL )
		return;

	free( rulbus_dac12_card );
	rulbus_dac12_card = NULL;
	rulbus_num_dac12_cards = 0;
}


/*----------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open( ) function, so it's not to be called by the
 * user directly)
 *----------------------------------------------------------------------*/

int rulbus_dac12_card_init( int handle )
{
	RULBUS_DAC12_CARD *tmp;
	unsigned int i;
	double Vmin;
	double dV;


	/* Evaluate the range and polarity settings for the card */

	if ( rulbus_card[ handle ].range == -1 )
		return RULBUS_NO_RNG;

	if ( rulbus_card[ handle ].polar == -1 )
		return RULBUS_NO_POL;

	for ( i = 0; i < sizeof ranges / sizeof *ranges; i++ )
		if ( ranges[ i ] ==
			 		( int ) floor( rulbus_card[ handle ].range * 1e2 + 0.5 ) )
			break;
	
	switch ( i )
	{
		case 0 :
			if ( rulbus_card[ handle ].polar != RULBUS_UNIPOLAR )
				return RULBUS_INV_RNG;
			Vmin = 0.0;
			dV = 20.0 / DAC12_RANGE;
			break;

		case 1:
			if ( rulbus_card[ handle ].polar == RULBUS_UNIPOLAR )
			{
				Vmin = 0.0;
				dV = 10.0 / DAC12_RANGE;
			}
			else
			{
				Vmin = -10.0;
				dV = 20.0 / DAC12_RANGE;
			}
			break;

		case 2 :
			if ( rulbus_card[ handle ].polar == RULBUS_UNIPOLAR )
			{
				Vmin = 0.0;
				dV = 5.0 / DAC12_RANGE;
			}
			else
			{
				Vmin = -5.0;
				dV = 10.0 / DAC12_RANGE;
			}
			break;

		case 3 :
			if ( rulbus_card[ handle ].polar != RULBUS_UNIPOLAR )
				return RULBUS_INV_RNG;
			Vmin = 0.0;
			dV = 5.0e-3;
			break;
			
		case 4:
			if ( rulbus_card[ handle ].polar == RULBUS_UNIPOLAR )
			{
				Vmin = 0.0;
				dV = 2.5e-3;
			}
			else
			{
				Vmin = -10.24;
				dV = 5.0e-3;
			}
			break;

		case 5 :
			if ( rulbus_card[ handle ].polar == RULBUS_UNIPOLAR )
			{
				Vmin = 0.0;
				dV = 1.25e-3;
			}
			else
			{
				Vmin = -5.12;
				dV = 2.5e-3;
			}
			break;

		default :
			return RULBUS_INV_RNG;
	}

	tmp = realloc( rulbus_dac12_card,
				   ( rulbus_num_dac12_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return RULBUS_NO_MEM;

	rulbus_dac12_card = tmp;
	tmp += rulbus_num_dac12_cards++;

	tmp->handle = handle;
	tmp->Vmin = Vmin;
	tmp->dV = dV;
	tmp->Vmax = DAC12_RANGE * tmp->dV + tmp->Vmin;
	tmp->v = DAC12_RANGE + 1;

	return RULBUS_OK;
}
	

/*----------------------------------------------------------------*
 * Function for deactivating a card (gets invoked automatically
 * by the rulbus_card_close( ) function, so it's not to be called
 * by the user directly)
 *----------------------------------------------------------------*/

void rulbus_dac12_card_exit( int handle )
{
	RULBUS_DAC12_CARD *card;


	/* Try to find the card, if it doesn't exist just return */

	if ( ( card = rulbus_dac12_card_find( handle ) ) == NULL )
		return;

	/* Remove the entry for the card */

	if ( card != rulbus_dac12_card + rulbus_num_dac12_cards - 1 )
		memcpy( card, card + 1, sizeof *card *
				rulbus_num_dac12_cards - ( card - rulbus_dac12_card ) - 1 );

	card = realloc( rulbus_dac12_card,
					( rulbus_num_dac12_cards - 1 ) * sizeof *card );

	if ( card != NULL )
		rulbus_dac12_card = card;

	rulbus_num_dac12_cards--;
}


/*--------------------------------------------------*
 * Function for enquiring about the DACs properties 
 *--------------------------------------------------*/

int rulbus_dac12_properties( int handle, double *Vmax, double *Vmin,
							 double *dV )
{
	RULBUS_DAC12_CARD *card;


	if ( ( card = rulbus_dac12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( Vmax )
		*Vmax = card->Vmax;

	if ( Vmin )
		*Vmin = card->Vmin;

	if ( dV )
		*dV = card->dV;

	return RULBUS_OK;
}


/*-------------------------------------------*
 * Function for setting a new output voltage
 *-------------------------------------------*/

int rulbus_dac12_set_voltage( int handle, double volts )
{
	RULBUS_DAC12_CARD *card;
	int retval;
	unsigned char byte;
	unsigned short int val;


	if ( ( card = rulbus_dac12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( volts + card->dV < card->Vmin || volts - card->dV > card->Vmax )
		return RULBUS_INV_VLT;

	val = ( unsigned short int )
							   ceil( ( volts - card->Vmin ) / card->dV - 0.5 );

	if ( card->v == val )
		return RULBUS_OK;

	rulbus_dac12_card[ handle ].v = val;

	byte = ( val >> 8 ) & 0xFF;
	if ( ( retval = rulbus_write( handle, DAC12_MSB, &byte, 1 ) ) < 0 )
		return retval;

	byte = val & 0xFF;
	return rulbus_write( handle, DAC12_LSB, &byte, 1 );
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_DAC12_CARD *rulbus_dac12_card_find( int handle )
{
	int i;


	if ( handle < 0 )
		return NULL;

	for ( i = 0; i < rulbus_num_dac12_cards; i++ )
		if ( handle == rulbus_dac12_card[ i ].handle )
			return rulbus_dac12_card + i;

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
