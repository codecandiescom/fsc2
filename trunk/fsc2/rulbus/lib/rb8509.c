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
	int nchan;
	double gain;
	double Vmax;
	double Vmin;
	double dV;
	int exttrg;
	int trig_mode;
	int data;
	unsigned char ctrl;
};


#define DATA_LOW_ADDR        0          /* read only */
#define STATUS_ADDR          1          /* read only */
#define CONTROL_ADDR         0          /* write only */
#define TRIGGER_ADDR         1          /* write only */


/* Bits in the control byte */

#define CTRL_CHANNEL_MASK              ( 7 << 0 )
#define CTRL_MULTIPLEXER_MASK          ( 1 << 3 )       /* keep always low ! */
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


static RULBUS_ADC12_CARD *rulbus_adc12_card = NULL;
static int rulbus_num_adc12_cards = 0;


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
	int retval = RULBUS_OK;
	unsigned char dummy;
	unsigned int i;
	double Vmin;
	double dV;


	/* Check if the number of channels of the card has been set and is
	   reasonable */

	if ( rulbus_card[ handle ].nchan < 0 )
		return RULBUS_MIS_CHN;
	else if ( rulbus_card[ handle ].nchan > RULBUS_ADC12_MAX_CHANNELS )
		return RULBUS_CHN_INV;
		
	/* Check that the polarity is specified */

	if ( rulbus_card[ handle ].polar == -1 )
		return RULBUS_NO_POL;

	/* Check the range setting and calculate the minumum voltage and
	   resolution (it looks as if the cards where built to allow outputting
	   an exact voltage of 0 V */

	if ( rulbus_card[ handle ].range <= 0.0 )
		return RULBUS_NO_RNG;

	if ( rulbus_card[ handle ].polar == RULBUS_UNIPOLAR )
	{
		dV = rulbus_card[ handle ].range / ADC12_RANGE;
		Vmin = 0.0;
	}
	else
	{
		dV = rulbus_card[ handle ].range / ( ADC12_RANGE >> 1 );
		Vmin = -rulbus_card[ handle ].range - dV;
	}

	if ( rulbus_card[ handle ].exttrg < 0 )
		return RULBUS_EXT_NO;

	tmp = realloc( rulbus_adc12_card,
				   ( rulbus_num_adc12_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return RULBUS_NO_MEM;

	rulbus_adc12_card = tmp;
	tmp += rulbus_num_adc12_cards++;

	tmp->handle = handle;
	tmp->nchan = rulbus_card[ handle ].nchan;
	tmp->Vmax = rulbus_card[ handle ].range;
	tmp->Vmin = Vmin;
	tmp->dV = dV;
	tmp->exttrg = rulbus_card[ handle ].exttrg;
	tmp->gain = 1.0;
	tmp->trig_mode = RULBUS_ADC12_INT_TRIG;
	tmp->ctrl = 0;

	/* Set a few defaults: selected channel is 0, gain is 1, use internal
	   trigger and no interrupts */

	if ( ( retval = rulbus_write( handle, CONTROL_ADDR, &tmp->ctrl, 1 ) ) < 0 )
	{
		tmp = realloc( rulbus_adc12_card,
					   --rulbus_num_adc12_cards * sizeof *tmp );
		if ( tmp == NULL )
			return RULBUS_NO_MEM;
		rulbus_adc12_card = tmp;
		return retval;
	}

	usleep( ADC12_DELAY );      /* allow for settling time of gain switching */

	/* Read the low data byte to make sure the EOC bit is cleared */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &dummy, 1 ) ) < 0 )
	{
		tmp = realloc( rulbus_adc12_card,
					   --rulbus_num_adc12_cards * sizeof *tmp );
		if ( tmp == NULL )
			return RULBUS_NO_MEM;
		rulbus_adc12_card = tmp;
	}

	return retval;
}
	

/*----------------------------------------------------------------*
 * Function for deactivating a card (gets invoked automatically
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


/*-----------------------------------------------------*
 * Function returns the number of channels of the card
 *-----------------------------------------------------*/

int rulbus_adc12_num_channels( int handle )
{
	RULBUS_ADC12_CARD *card;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	return card->nchan;
}


/*-----------------------------------------------------*
 * Function returns the number of channels of the card
 *-----------------------------------------------------*/

int rulbus_adc12_has_external_trigger( int handle )
{
	RULBUS_ADC12_CARD *card;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	return card->exttrg;
}


/*---------------------------------------------------*
 * Function for selecting which channnel of the card
 * is to be used in conversions
 *---------------------------------------------------*/

int rulbus_adc12_set_channel( int handle, int channel )
{
	RULBUS_ADC12_CARD *card;
	unsigned char ctrl;
	int retval;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( channel < 0 || channel >= card->nchan )
		return RULBUS_INV_ARG;

	ctrl = ( card->ctrl & ~ CTRL_CHANNEL_MASK ) | channel;

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	if ( ( retval = rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 ) )
		 																  < 0 )
		return retval;

	/* Read the low data byte to make sure the EOC bit is cleared */

	return rulbus_read( handle, DATA_LOW_ADDR, &ctrl, 1 );
}


/*----------------------------------------------------------------*
 * Function for setting the gain of the card. Available gains are
 * either 1, 2, 4 or 8.
 *----------------------------------------------------------------*/

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

	card->gain = ( double ) gain;

	while ( gain >>= 1 )
		gain_bits++;

	gain_bits <<= CTRL_GAIN_SHIFT;

	ctrl = ( card->ctrl & ~ CTRL_GAIN_MASK ) | gain_bits;

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	if ( ( retval = rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 ) )
																		  < 0 )
		return retval;

	/* The card needs a settling time of about 18 us after gain switching */

	usleep( ADC12_DELAY );

	/* Read the low data byte to make sure the EOC bit is cleared */

	return rulbus_read( handle, DATA_LOW_ADDR, &ctrl, 1 );
}


/*-------------------------------------------------------------*
 * Function for setting the trigger mode of the card to either
 * RULBUS_ADC12_INT_TRIG or RULBUS_ADC12_INT_TRIG
 *-------------------------------------------------------------*/

int rulbus_adc12_set_trigger_mode( int handle, int mode )
{
	RULBUS_ADC12_CARD *card;
	unsigned char ctrl;
	int retval;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( mode != RULBUS_ADC12_INT_TRIG && mode != RULBUS_ADC12_EXT_TRIG )
		return RULBUS_INV_ARG;

	/* Some cards can't be triggered externally */

	if ( mode != RULBUS_ADC12_INT_TRIG && ! card->exttrg )
		return RULBUS_NO_EXT;

	if ( mode == RULBUS_ADC12_INT_TRIG )
		ctrl = card->ctrl & ~ CTRL_EXT_TRIGGER_ENABLE;
	else
		ctrl = card->ctrl | CTRL_EXT_TRIGGER_ENABLE;

	if ( ctrl == card->ctrl )
		return RULBUS_OK;

	card->ctrl = ctrl;
	card->trig_mode = mode;

	if ( ( retval = rulbus_write( handle, CONTROL_ADDR, &card->ctrl, 1 ) )
																		  < 0 )
		return retval;

	/* Read the low data byte to make sure the EOC bit is cleared */

	return rulbus_read( handle, DATA_LOW_ADDR, &ctrl, 1 );
}


/*---------------------------------------------------------------------*
 * Function to determine the minimum and maximum input voltage as well
 * as the voltage resolution of the card.
 *---------------------------------------------------------------------*/

int rulbus_adc12_properties( int handle, double *Vmax, double *Vmin,
							 double *dV )

{
	RULBUS_ADC12_CARD *card;


	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	if ( Vmax != NULL )
		*Vmax = card->Vmax;

	if ( Vmin != NULL )
		*Vmin = card->Vmin;

	if ( dV != NULL )
		*dV = card->dV;

	return RULBUS_OK;
}


/*----------------------------------------------------------------------*
 * Function for testing if a conversion is already finished. It returns
 * 1 and stores the voltage in what 'volts' points to if a conversion
 * has already happened, otherwise 0. Since it doesn't make sense to
 * call the function when in internal trigger mode, in this case always
 * 0 is returned.
 *----------------------------------------------------------------------*/

int rulbus_adc12_check_convert( int handle, double *volts )
{
	RULBUS_ADC12_CARD *card;
	unsigned char hi, low;
	int retval;


	if ( volts == NULL )
		return RULBUS_INV_ARG;

	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	/* Return immediately if we're in internal trigger mode (or if this is
	   a card that can't be triggered externally) */

	if ( card->trig_mode == RULBUS_ADC12_INT_TRIG )
		return 0;

	/* Check if a conversion has already happened which also means that an
	   external trigger was received */

	if ( ( retval = rulbus_read( handle, STATUS_ADDR, &hi, 1 ) ) < 0 )
		return retval;

	if ( ! ( hi & STATUS_EOC ) )
		return 0;

	/* If the conversion is already done also get the low data byte and
	   return the value to the user */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &low, 1 ) ) < 0 )
		return retval;

	/* Calculate the voltage from the data we just received */

	card->data = ( ( hi & STATUS_DATA_MASK ) << 8 ) | low;
	*volts = ( card->dV * card->data - card->Vmin ) / card->gain;
	return 1;
}


/*--------------------------------------------------------*
 * Function for getting a converted voltage from the card.
 *--------------------------------------------------------*/

int rulbus_adc12_convert( int handle, double *volts )
{
	RULBUS_ADC12_CARD *card;
	int retval;
	unsigned char trig;
	unsigned char hi, low;


	if ( volts == NULL )
		return RULBUS_INV_ARG;

	if ( ( card = rulbus_adc12_card_find( handle ) ) == NULL )
		return RULBUS_INV_HND;

	/* If we're in internal trigger mode trigger a conversion (after making
	   sure EOC is reset). The value we write to the trigger register is of
	   no importance, the mere act of writing to the register triggers. */

	if ( card->trig_mode == RULBUS_ADC12_INT_TRIG &&
		 ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &trig, 1 ) ) <  0 ||
		   ( retval = rulbus_write( handle, TRIGGER_ADDR, &trig, 1 ) ) < 0 ) )
		return retval;

	/* Check the EOC (end of conversion) bit */

	do
	{
		if ( ( retval = rulbus_read( handle, STATUS_ADDR, &hi, 1 ) ) < 0 )
			return retval;
	} while ( ! ( hi & STATUS_EOC ) );

	/* Get the low data byte */

	if ( ( retval = rulbus_read( handle, DATA_LOW_ADDR, &low, 1 ) ) < 0 )
		return retval;

	/* Calculate the voltage from the data we just received */

	card->data = ( ( hi & STATUS_DATA_MASK ) << 8 ) | low;
	*volts = ( card->dV * card->data - card->Vmin ) / card->gain;
	return RULBUS_OK;
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_ADC12_CARD *rulbus_adc12_card_find( int handle )
{
	int i;


	if ( handle < 0 )
		return NULL;

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
