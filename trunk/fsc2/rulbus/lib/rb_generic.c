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
 *  To contact the author send email to
 *  Jens.Toerring@physik.fu-berlin.de
 */


#include "rulbus_lib.h"


typedef struct RULBUS_GENERIC_CARD RULBUS_GENERIC_CARD;

struct RULBUS_GENERIC_CARD {
	int handle;
};


static RULBUS_GENERIC_CARD *rulbus_generic_card = NULL;
static int rulbus_num_generic_cards = 0;


static RULBUS_GENERIC_CARD *rulbus_generic_card_find( int handle );


/*------------------------------------------------------------------*
 * Function for initializing the generic card subsystem (gets invoked
 * automatically by the rulbus_open() function, so it's not to be
 * called by the user directly)
 *------------------------------------------------------------------*/

int rulbus_generic_init( void )
{
	rulbus_generic_card = NULL;
	rulbus_num_generic_cards = 0;
	return RULBUS_OK;
}


/*------------------------------------------------------------------*
 * Function for deactivating the generic card subsystem (gets invoked
 * automatically by the rulbus_close() function, so it's not to be
 * called by the user directly). Usually it won't even do anything,
 * at least if rulbus_generic_card_exit() has been called for all
 * existing cards.
 *------------------------------------------------------------------*/

void rulbus_generic_exit( void )
{
	if ( rulbus_generic_card == NULL )
		return;

	free( rulbus_generic_card );
	rulbus_generic_card = NULL;
	rulbus_num_generic_cards = 0;
}


/*---------------------------------------------------------------------*
 * Function for initializing a single card (gets invoked automatically
 * by the rulbus_card_open() function, so it's not to be called by the
 * user directly)
 *---------------------------------------------------------------------*/

int rulbus_generic_card_init( int handle )
{
	RULBUS_GENERIC_CARD *tmp;


	tmp = realloc( rulbus_generic_card,
				   ( rulbus_num_generic_cards + 1 ) * sizeof *tmp );

	if ( tmp == NULL )
		return RULBUS_NO_MEMORY;

	rulbus_generic_card = tmp;
	tmp += rulbus_num_generic_cards++;

	tmp->handle = handle;

	return RULBUS_OK;
}
	

/*---------------------------------------------------------------*
 * Function for deactivation a card (gets invoked automatically
 * by the rulbus_card_close() function, so it's not to be called
 * by the user directly)
 *---------------------------------------------------------------*/

int rulbus_generic_card_exit( int handle )
{
	RULBUS_GENERIC_CARD *card;


	/* Try to find the card, if it doesn't exist just return */

	if ( ( card = rulbus_generic_card_find( handle ) ) == NULL )
		return rulbus_errno = RULBUS_OK;

	/* Remove the entry for the card */

	if ( rulbus_num_generic_cards > 1 )
	{
		if ( card != rulbus_generic_card + rulbus_num_generic_cards - 1 )
			memmove( card, card + 1, sizeof *card *
					 ( rulbus_num_generic_cards -
					   ( card - rulbus_generic_card ) - 1 ) );

		card = realloc( rulbus_generic_card,
						( rulbus_num_generic_cards - 1 ) * sizeof *card );

		if ( card == NULL )
			return RULBUS_NO_MEMORY;

		rulbus_generic_card = card;
	}
	else
	{
		free( rulbus_generic_card );
		rulbus_generic_card = NULL;
	}

	rulbus_num_generic_cards--;

	return rulbus_errno = RULBUS_OK;
}


/*----------------------------------------------*
 * Function for writing data to a generic card
 *---------------------------------------------*/

int rulbus_generic_write( int handle, unsigned char address,
						  unsigned char *data, size_t len )
{
	RULBUS_GENERIC_CARD *card;


	if ( address == 0 || address > RULBUS_MAX_CARD_ADDR ||
		 data == NULL || len == 0 )
		return RULBUS_INVALID_ARGUMENT;

	if ( ( card = rulbus_generic_card_find( handle ) ) == NULL )
		return RULBUS_INVALID_CARD_HANDLE;

	return rulbus_write( handle, address, data, len );
}


/*-----------------------------------------------*
 * Function for reading data from a generic card
 *-----------------------------------------------*/

int rulbus_generic_read( int handle, unsigned char address,
						 unsigned char *data, size_t len )
{
	RULBUS_GENERIC_CARD *card;


	if ( address == 0 || address > RULBUS_MAX_CARD_ADDR ||
		 data == NULL || len == 0 )
		return RULBUS_INVALID_ARGUMENT;

	if ( ( card = rulbus_generic_card_find( handle ) ) == NULL )
		return RULBUS_INVALID_CARD_HANDLE;

	return rulbus_write( handle, address, data, len );
}


/*----------------------------------------------------*
 * Function for finding a cards entry from its handle
 *----------------------------------------------------*/

static RULBUS_GENERIC_CARD *rulbus_generic_card_find( int handle )
{
	int i;


	if ( handle < 0 )
		return NULL;

	for ( i = 0; i < rulbus_num_generic_cards; i++ )
		if ( handle == rulbus_generic_card[ i ].handle )
			return rulbus_generic_card + i;

	return NULL;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
