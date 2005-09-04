/*
 *  $Id$
 *
 *  Copyright (C) 2003-2005 Jens Thoms Toerring
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
 *  To contact the author send email to jtt@toerring.de
 */


#include "rulbus_lib.h"


typedef struct RULBUS_RB8509_ADC12_CARD RULBUS_RB8509_ADC12_CARD;

struct RULBUS_RB8509_ADC12_CARD {
	int           handle;
	int           num_channels;
	double        gain;
	double        Vmax;
	double        Vmin;
	double        dV;
	int           has_ext_trigger;
	int           trig_mode;
	int           data;
	unsigned char ctrl;
};


#define DATA_LOW_ADDR        0          /* read only */
#define STATUS_ADDR          1          /* read only */
#define CONTROL_ADDR         0          /* write only */
#define TRIGGER_ADDR         1          /* write only */


/* Bits in the control byte */

#define CTRL_CHANNEL_MASK              ( 7 << 0 )
#define CTRL_MULTIPLEXER               ( 1 << 3 )       /* active low ! */
#define CTRL_GAIN_MASK                 ( 3 << 4 )
#define CTRL_EXT_TRIGGER_ENABLE        ( 1 << 6 )
#define CTRL_INTERRUPT_ENABLE          ( 1 << 7 )
#define CTRL_GAIN_SHIFT                4


/* Bits in the status byte */

#define STATUS_DATA_MASK               ( 0x0F << 0 )
#define STATUS_STS                     (    1 << 4 )
#define STATUS_EOC                     (    1 << 5 )
#define STATUS_INTERRUPTT              (    1 << 6 )
#define STATUS_EXT_TRIGGER             (    1 << 7 )


#define ADC12_RANGE                    0x0FFF

#define ADC12_DELAY                    18               /* 18 us */


static RULBUS_RB8509_ADC12_CARD *rulbus_rb8509_adc12_card = NULL;
static int rulbus_num_adc12_cards = 0;


static RULBUS_RB8509_ADC12_CARD *rulbus_rb8509_adc12_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the adc12 card subsystem (gets invoked
 * automatically by the rulbus_open() function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_rb8509_adc12_init( void )
{
	rulbus_rb8509_adc12_card = NULL;
	rulbus_num_adc12_cards = 0;
	return rulbus_errno = RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the adc12 card subsystem (gets invoked
 * automatically by the rulbus_close() function, so it's not to be
 * called by the user directly). Usually it won't even do anything,
 * at least if rulbus_rb8509_adc12_card_exit() has been called for
 * all existing cards.
 *------------------------------------------------------------------*/

void rulbus_rb8509_adc12_exit( void )
{
	if ( rulbus_rb8509_adc12_card == NULL )
		return;

	free( rulbus_rb8509_adc12_card );
	rulbus_rb8509_adc12_card = NULL;
	rulbus_num_adc12_cards = 0;
}


/*---------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open() function, so it's not to be called by the
 * user directly)
 *---------------------------------------------------------------------*/

int rulbus_rb8509_adc12_card_init( int handle )
{
	RULBUS_RB8509_ADC12_CARD *tmp;
	int retval = RULBUS_OK;
	unsigned char dummy;


	tmp = realloc( rulbus_rb8509_adc12_card,
				   ( rulbus_num_adc12_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return rulbus_errno = RULBUS_NO_MEMORY;

	rulbus_rb8509_adc12_card = tmp;
	tmp += rulbus_num_adc12_cards++;

	tmp->handle = handle;
	tmp->num_channels = rulbus_card[ handle ].num_channels;
	tmp->dV = rulbus_card[ handle ].vpb;

	if ( rulbus_card[ handle ].bipolar )
	{
		tmp->Vmax = tmp->dV * ( ADC12_RANGE >> 1 );
		tmp->Vmin = tmp->Vmax - tmp->dV * ADC12_RANGE;
	}
	else
	{
		tmp->Vmax = tmp->dV * ADC12_RANGE;
		tmp->Vmin = 0.0;
	}

	tmp->has_ext_trigger = rulbus_card[ handle ].has_ext_trigger;
	tmp->gain = 1.0;
	tmp->trig_mode = RULBUS_RB8509_ADC12_INT_TRIG;
	tmp->ctrl = 0;

	/* Set a few defaults: selected channel is 0, gain is 1, use internal
	   trigger and no interrupts */

	if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
								  &tmp->ctrl, 1 ) ) != 1 )
	{
		tmp = realloc( rulbus_rb8509_adc12_card,
					   --rulbus_num_adc12_cards * sizeof *tmp );
		if ( tmp == NULL )
			return rulbus_errno = RULBUS_NO_MEMORY;
		rulbus_rb8509_adc12_card = tmp;
		return rulbus_errno = retval;
	}

	usleep( ADC12_DELAY );      /* allow for settling time after gain switch */

	/* Read the low data byte to make sure the EOC bit is cleared */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &dummy, 1 ) ) != 1 )
	{
		tmp = realloc( rulbus_rb8509_adc12_card,
					   --rulbus_num_adc12_cards * sizeof *tmp );
		if ( tmp == NULL )
			return rulbus_errno = RULBUS_NO_MEMORY;
		rulbus_rb8509_adc12_card = tmp;
		return rulbus_errno = retval;
	}

	rulbus_errno = RULBUS_OK;
	return retval;
}


/*----------------------------------------------------------------*
 * Function for deactivating a card (gets invoked automatically
 * by the rulbus_card_close() function, so it's not to be called
 * by the user directly)
 *----------------------------------------------------------------*/

int rulbus_rb8509_adc12_card_exit( int handle )
{
	RULBUS_RB8509_ADC12_CARD *card;


	/* Try to find the card, if it doesn't exist just return */

	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_OK;

	/* Remove the entry for the card */

	if ( rulbus_num_adc12_cards > 1 )
	{
		if ( card != rulbus_rb8509_adc12_card + rulbus_num_adc12_cards - 1 )
			memmove(  card, card + 1, sizeof *card *
					  ( rulbus_num_adc12_cards -
						( card - rulbus_rb8509_adc12_card ) - 1 ) );

		card = realloc( rulbus_rb8509_adc12_card,
						( rulbus_num_adc12_cards - 1 ) * sizeof *card );

		if ( card == NULL )
			return rulbus_errno = RULBUS_NO_MEMORY;

		rulbus_rb8509_adc12_card = card;
	}
	else
	{
		free( rulbus_rb8509_adc12_card );
		rulbus_rb8509_adc12_card = NULL;
	}

	rulbus_num_adc12_cards--;

	return rulbus_errno = RULBUS_OK;
}


/*-----------------------------------------------------*
 * Function returns the number of channels of the card
 *-----------------------------------------------------*/

int rulbus_rb8509_adc12_num_channels( int handle )
{
	RULBUS_RB8509_ADC12_CARD *card;


	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	rulbus_errno = RULBUS_OK;
	return card->num_channels;
}


/*---------------------------------------------------*
 * Function for selecting which channnel of the card
 * is to be used in conversions
 *---------------------------------------------------*/

int rulbus_rb8509_adc12_set_channel( int handle, int channel )
{
	RULBUS_RB8509_ADC12_CARD *card;
	unsigned char ctrl;
	int retval;


	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( channel < 0 || channel >= card->num_channels )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	ctrl = ( card->ctrl & ~ CTRL_CHANNEL_MASK ) | channel;

	if ( ctrl == card->ctrl )
		return rulbus_errno = RULBUS_OK;

	card->ctrl = ctrl;
	if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
								  &card->ctrl, 1 ) ) != 1 )
		return rulbus_errno = retval;

	/* Read the low data byte to make sure the EOC bit is cleared */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &ctrl, 1 ) ) != 1 )
		return rulbus_errno = retval;

	return rulbus_errno = RULBUS_OK;
}


/*----------------------------------------------------------------*
 * Function for setting the gain of the card. Available gains are
 * either 1, 2, 4 or 8.
 *----------------------------------------------------------------*/

int rulbus_rb8509_adc12_set_gain( int handle, int gain )
{
	RULBUS_RB8509_ADC12_CARD *card;
	unsigned char ctrl;
	unsigned char gain_bits = 0;
	int retval;


	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( gain != 1 && gain != 2 && gain != 4 && gain != 8 )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	card->gain = ( double ) gain;

	while ( gain >>= 1 )
		gain_bits++;

	gain_bits <<= CTRL_GAIN_SHIFT;

	ctrl = ( card->ctrl & ~ CTRL_GAIN_MASK ) | gain_bits;

	if ( ctrl == card->ctrl )
		return rulbus_errno = RULBUS_OK;

	card->ctrl = ctrl;
	if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
								  &card->ctrl, 1 ) ) != 1 )
		return rulbus_errno = retval;

	/* The card needs a settling time of about 18 us after gain switching */

	usleep( ADC12_DELAY );

	/* Read the low data byte to make sure the EOC bit is cleared */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &ctrl, 1 ) ) != 1 )
		 return rulbus_errno = retval;

	return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------------------------*
 * Function for setting the trigger mode of the card to either
 * RULBUS_RB8509_ADC12_INT_TRIG or RULBUS_RB8509_ADC12_INT_TRIG
 *-------------------------------------------------------------*/

int rulbus_rb8509_adc12_set_trigger_mode( int handle, int mode )
{
	RULBUS_RB8509_ADC12_CARD *card;
	unsigned char ctrl;
	int retval;


	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( mode != RULBUS_RB8509_ADC12_INT_TRIG &&
		 mode != RULBUS_RB8509_ADC12_EXT_TRIG )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	/* Some cards can't be triggered externally */

	if ( mode != RULBUS_RB8509_ADC12_INT_TRIG && ! card->has_ext_trigger )
		return rulbus_errno = RULBUS_NO_EXT_TRIGGER;

	if ( mode == RULBUS_RB8509_ADC12_INT_TRIG )
		ctrl = card->ctrl & ~ CTRL_EXT_TRIGGER_ENABLE;
	else
		ctrl = card->ctrl | CTRL_EXT_TRIGGER_ENABLE;

	if ( ctrl == card->ctrl )
		return rulbus_errno = RULBUS_OK;

	card->ctrl = ctrl;
	card->trig_mode = mode;

	if ( ( retval = rulbus_write( handle, CONTROL_ADDR,
								  &card->ctrl, 1 ) ) != 1 )
		return rulbus_errno = retval;

	/* Read the low data byte to make sure the EOC bit is cleared */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &ctrl, 1 ) ) != 1 )
		return rulbus_errno = retval;

	return rulbus_errno = RULBUS_OK;
}


/*---------------------------------------------------------------------*
 * Function to determine the minimum and maximum input voltage as well
 * as the voltage resolution of the card.
 *---------------------------------------------------------------------*/

int rulbus_rb8509_adc12_properties( int handle, double *Vmax, double *Vmin,
									double *dV )

{
	RULBUS_RB8509_ADC12_CARD *card;


	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( Vmax != NULL )
		*Vmax = card->Vmax;

	if ( Vmin != NULL )
		*Vmin = card->Vmin;

	if ( dV != NULL )
		*dV = card->dV;

	return rulbus_errno = RULBUS_OK;
}


/*-----------------------------------------------------*
 * Function returns the number of channels of the card
 *-----------------------------------------------------*/

int rulbus_rb8509_adc12_has_external_trigger( int handle )
{
	RULBUS_RB8509_ADC12_CARD *card;


	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	rulbus_errno = RULBUS_OK;
	return ( int ) card->has_ext_trigger;
}


/*----------------------------------------------------------------------*
 * Function for testing if a conversion is already finished. It returns
 * 1 and stores the voltage in what 'volts' points to if a conversion
 * has already happened, otherwise 0. Since it doesn't make sense to
 * call the function when in internal trigger mode, in this case always
 * 0 is returned.
 *----------------------------------------------------------------------*/

int rulbus_rb8509_adc12_check_convert( int handle, double *volts )
{
	RULBUS_RB8509_ADC12_CARD *card;
	unsigned char hi, low;
	int retval;


	if ( volts == NULL )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	/* Return immediately if we're in internal trigger mode */

	if ( card->trig_mode == RULBUS_RB8509_ADC12_INT_TRIG )
	{
		rulbus_errno = RULBUS_OK;
		return 0;
	}

	/* Check if a conversion has already happened which also means that an
	   external trigger was received */

	if ( ( retval = rulbus_read( handle, STATUS_ADDR, &hi, 1 ) ) != 1 )
		return rulbus_errno = retval;

	if ( ! ( hi & STATUS_EOC ) )
	{
		rulbus_errno = RULBUS_OK;
		return 0;
	}

	/* If the conversion is already done also get the low data byte and
	   return the value to the user */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &low, 1 ) ) != 1 )
		return rulbus_errno = retval;

	/* Calculate the voltage from the data we just received */

	card->data = ( ( hi & STATUS_DATA_MASK ) << 8 ) | low;
	*volts = ( card->dV * card->data + card->Vmin ) / card->gain;
	rulbus_errno = RULBUS_OK;
	return 1;
}


/*--------------------------------------------------------*
 * Function for getting a converted voltage from the card.
 *--------------------------------------------------------*/

int rulbus_rb8509_adc12_convert( int handle, double *volts )
{
	RULBUS_RB8509_ADC12_CARD *card;
	int retval;
	unsigned char trig;
	unsigned char hi, low;


	if ( volts == NULL )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	if ( ( card = rulbus_rb8509_adc12_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	/* If we're in internal trigger mode trigger a conversion (after making
	   sure EOC is reset). The value we write to the trigger register is of
	   no importance, the mere act of writing to the register triggers. */

	if ( card->trig_mode == RULBUS_RB8509_ADC12_INT_TRIG &&
		 ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &trig, 1 ) ) != 1 ||
		   ( retval = rulbus_write( handle, TRIGGER_ADDR, &trig, 1 ) ) != 1 ) )
		return rulbus_errno = retval;

	/* Check the EOC (end of conversion) bit */

	do
	{
		if ( ( retval = rulbus_read( handle, STATUS_ADDR, &hi, 1 ) ) != 1 )
			return rulbus_errno = retval;
	} while ( ! ( hi & STATUS_EOC ) );

	/* Get the low data byte (thereby also resetting the EOC bit) */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &low, 1 ) ) != 1 )
		return rulbus_errno = retval;

	/* Calculate the voltage from the data we just received */

	card->data = ( ( hi & STATUS_DATA_MASK ) << 8 ) | low;
	*volts = ( card->dV * card->data + card->Vmin ) / card->gain;
	return rulbus_errno = RULBUS_OK;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_RB8509_ADC12_CARD *rulbus_rb8509_adc12_card_find( int handle )
{
	int i;


	if ( handle < 0 )
		return NULL;

	for ( i = 0; i < rulbus_num_adc12_cards; i++ )
		if ( handle == rulbus_rb8509_adc12_card[ i ].handle )
			return rulbus_rb8509_adc12_card + i;

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
