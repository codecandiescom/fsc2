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


typedef struct RULBUS_ADC12_CARD RULBUS_ADC12_CARD;

struct RULBUS_ADC12_CARD {
	int handle;
	double gain;
	unsigned char ctrl;
};


#define ADC12_MAX_CHANNELS   7          /* could als be 15 ! */


#define DATA_LOW             0          /* read only */
#define STATUS_ADDR          1          /* read only */
#define CONTROL_ADDR         0          /* write only */
#define TRIGGER_ADDR         1          /* write only */


/* Bits in the control byte */

#define CTRL_CHANNEL_MASK              ( 0x0F << 0 )
#define CTRL_GAIN_SHIFT                4
#define CTRL_GAIN_MASK                 (    3 << 4 )
#define CTRL_EXT_TRIGGER_ENABLE        (    1 << 6 )
#define CTRL_INTERRUPT_ENABLE          (    1 << 7 )


/* Bits in the status byte */

#define STATUS_DATA_MASK               ( 0x0F << 0 )
#define STATUS_STS                     (    1 << 4 )
#define STATUS_EOC                     (    1 << 5 )
#define STATUS_INTERRUPTT              (    1 << 6 )
#define STATUS_EXT_TRIGGER             (    1 << 7 )


#define VOLTAGE_SPAN                   20.0   /* 20 V */
#define ADC12_RANGE                    ( 1 << 12 )
#define ADC12_OFFSET                   ( 1 << 11 )


#define ADC12_DELAY                    18   /* 18 us */


static RULBUS_ADC12_CARD *rulbus_adc12_card;
static int rulbus_num_adc12_cards;


static RULBUS_ADC12_CARD *rulbus_adc12_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the adc12 card subsystem (gets invoked
 * automatically by the rulbus_open( ) function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_adc12_init( void )
{
	rulbus_adc12_card = NULL;
	rulbus_num_adc12_cards = 0;
	return RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the adc12 card subsystem (gets invoked
 * automatically by the rulbus_close( ) function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

void rulbus_adc12_exit( void )
{
	if ( rulbus_adc12_card == NULL )
		return;

	free( rulbus_adc12_card );
	rulbus_adc12_card = NULL;
	rulbus_num_adc12_cards = 0;
}


/*----------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open( ) function, so it's not to be called by the
 * user directly)
 *----------------------------------------------------------------------*/

int rulbus_adc12_card_init( int handle )
{
	RULBUS_ADC12_CARD *tmp;


	tmp = realloc( rulbus_adc12_card,
				   ( rulbus_num_adc12_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return RULBUS_NO_MEM;

	rulbus_adc12_card = tmp;
	tmp += rulbus_num_adc12_cards++;

	tmp->handle = handle;
	tmp->ctrl = 0;

	/* Set a few defaults: selected channel is 0, gain is 1, no external
	   trigger and no interrupts */

	return rulbus_write( handle, CONTROL_ADDR, &tmp->ctrl, 1 );
}
	

/*----------------------------------------------------------------*
 * Function for deactivation a card (gets invoked automatically
 * by the rulbus_card_close( ) function, so it's not to be called
 * by the user directly)
 *----------------------------------------------------------------*/

void rulbus_adc12_card_exit( int handle )
{
	RULBUS_ADC12_CARD *card;


	/* Try to find the card, if it doesn't exist just return */

	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return;

	/* Remove the entry for the card */

	if ( card != rulbus_adc12_card + rulbus_num_adc12_cards - 1 )
		memcpy( card, card + 1, sizeof *card *
				rulbus_num_adc12_cards - ( card - rulbus_adc12_card ) - 1 );

	card = realloc( rulbus_adc12_card,
					( rulbus_num_adc12_cards - 1 ) * sizeof *card );

	if ( card != NULL )
		rulbus_adc12_card = card;

	rulbus_num_adc12_cards--;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int rulbus_adc12_set_channel( int handle, int channel )
{
	RULBUS_ADC12_CARD *card;
	unsigned char ctrl;
	int retval;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( channel < 0 || channel > ADC12_MAX_CHANNELS )
		return RULBUS_INV_ARG;

	ctrl = ( card->ctrl & ~ CTRL_CHANNEL_MASK ) | channel;

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	retval = rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 );

	usleep( ADC12_DELAY );
	return retval;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int rulbus_adc12_set_gain( int handle, int gain )
{
	RULBUS_ADC12_CARD *card;
	unsigned char ctrl;
	unsigned char gain_bits = 0;
	int retval;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( gain != 1 && gain != 2 && gain != 4 && gain != 8 )
		return RULBUS_INV_ARG;

	card->gain = 1.0 / gain;

	while ( gain >>= 1 )
		gain_bits++;

	gain_bits <<= CTRL_GAIN_SHIFT;

	ctrl = ( card->ctrl & ~ CTRL_GAIN_MASK ) | gain_bits;

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	retval = rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 );

	usleep( ADC12_DELAY );
	return retval;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int rulbus_adc12_convert( int handle, double *volts )
{
	RULBUS_ADC12_CARD *card;
	int retval;
	unsigned char trig = 0xFF;
	unsigned char high, low;
	int data;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	/* Trigger a conversion */

	if ( ( retval = rulbus_write( handle, TRIGGER_ADDR, &trig, 1 ) ) < 0 )
		return retval;

	/* Wait for the EOC (end of conversion) bit to become set */

	do {
		if ( ( retval = rulbus_read( handle, STATUS_ADDR, &high, 1 ) ) < 0 )
			return retval;
	} while ( ! ( high & STATUS_EOC ) );

	/* Get the low data byte */

	if ( ( retval = rulbus_read( handle, DATA_LOW, &low, 1 ) ) < 0 )
		return retval;

	/* Calculate the voltage from the dat we just received */

	data = ( ( ( high & STATUS_DATA_MASK ) << 8 ) | low ) - ADC12_OFFSET;

	*volts = ( VOLTAGE_SPAN / ADC12_RANGE ) * card->gain * data;
	return RULBUS_OK;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_ADC12_CARD *rulbus_adc12_card_find( int handle )
{
	int i;


	for ( i = 0; i < rulbus_num_adc12_cards; i++ )
		if ( handle == rulbus_adc12_card[ i ].handle )
			return rulbus_adc12_card + i;

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
