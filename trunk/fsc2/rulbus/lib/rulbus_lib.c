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


#include "rulbus.h"
#include "rulbus_epp.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


RULBUS_CARD_LIST *rulbus_card = NULL;
int rulbus_num_cards = 0;
char *rulbus_dev_file = NULL;
int ni_daq_errno = RULBUS_OK;


static const char *rulbus_errlist[ ] = {
	"Success",                                         /* RULBUS_OK      */
    "Can't open configuration file",                   /* RULBUS_OPN_CFG */
	"Can't open device file",                          /* RULBUS_OPN_DVF */
	"Syntax error in configuration file",              /* RULBUS_STX_ERR */
	"Running out of memory",                           /* RULBUS_NO_MEM  */
	"Too many racks listed in configuration file",     /* RULBUS_RCK_CNT */
	"Duplicate rack address in configuration file",    /* RULBUS_RCK_DUP */
	"Missing rack address in configuration file",      /* RULBUS_NO_RCK  */
	"Invalid rack address in configuration file",      /* RULBUS_INV_RCK */
	"More than one device file in configuration file", /* RULBUS_DVF_CNT */
	"Too many cards in configuration file",            /* RULBUS_MAX_CRD */
	"Invalid card address in configuration file",      /* RULBUS_CRD_ADD */
	"More than one address for a card",                /* RULBUS_ADD_DUP */
	"More than one type for a card",                   /* RULBUS_TYP_DUP */
	"Too many cards for single rack",                  /* RULBUS_CRD_CNT */
	"No cards found in configuration file",            /* RULBUS_NO_CRD  */
	"Missing card name in configuration file",         /* RULBUS_CRD_NAM */
	"Invalid card type in configuration file",         /* RULBUS_INV_TYP */
	"Address confict for cards in same rack",          /* RULBUS_ADD_CFL */
	"Missing intialization of library",                /* RULBUS_NO_INIT */
	"Invalid function argument",                       /* RULBUS_INV_ARG */
	"No such card",                                    /* RULBUS_INV_CRD  */
	"Invalid card handle",                             /* RULBUS_INV_HND */
	"Card has not been opened",                        /* RULBUS_CRD_NOP */
	"Invalid card address offset",                     /* RULBUS_INV_OFF */
};

static const int rulbus_nerr =
                ( int ) ( sizeof rulbus_errlist / sizeof rulbus_errlist[ 0 ] );

static bool rulbus_in_use = UNSET;
static int fd;


static int rulbus_check_config( void );
static void rulbus_cleanup( void );
static int rulbus_write_rack( unsigned char rack, unsigned char addr,
							  unsigned char data );
static int rulbus_read_rack( unsigned char rack, unsigned char addr,
							 unsigned char *data );

extern int yyparse( void );


/*--------------------------------------------------------------------*
 * This function has to be called before any other function from the
 * RULBUS library can be used. It will first try to read in the RULBUS
 * configuration file (usually it will be /etc/rulbus.conf) to find
 * out about the cards in the racks.
 *--------------------------------------------------------------------*/

int rulbus_open( void )
{
	const char *config_name;
	extern FILE *yyin;         /* fron the parser */
	int retval;


	/* Don't try to reinitialize */

	if ( rulbus_in_use )
		return rulbus_errno = RULBUS_OK;

	/* Figure out the name of the configuration file and try to open it */

	if ( ( config_name = getenv( "RULBUS_CONFIG_FILE" ) ) == NULL )
		config_name = RULBUS_DEFAULT_CONFIG_FILE;

	if ( ( yyin = fopen( config_name, "r" ) ) == NULL )
		return rulbus_errno = RULBUS_OPN_CFG;

	/* Parse the configuration file */

    if ( ( retval = yyparse( ) ) != RULBUS_OK )
	{
		fclose( yyin );
		rulbus_cleanup( );
		return rulbus_errno = retval;
    }

    fclose( yyin );

	/* Check that the data from the configuration file were reasonable */

	if ( ( retval = rulbus_check_config( ) ) != RULBUS_OK )
	{
		rulbus_cleanup( );
		return rulbus_errno = retval;
	}

	if ( ( fd = open( rulbus_dev_file, O_RDWR ) ) < 0 )
	{
		rulbus_cleanup( );
		return rulbus_errno = RULBUS_OPN_DVF;
	}

	rulbus_in_use = SET;

	return rulbus_errno = RULBUS_OK;
}


/*-----------------------------------*
 * Function to "close" to the RULBUS
 *-----------------------------------*/

int rulbus_close( void )
{
	if ( rulbus_in_use )
	{
		close( fd );
		rulbus_cleanup( );
		rulbus_in_use = UNSET;
	}

	return rulbus_errno = RULBUS_OK;
}
	

/*----------------------------------------------------------------*
 * Prints out a string to stderr, consisting of a user supplied
 * string (the argument of the function), a colon, a blank and
 * a short descriptive text of the error encountered in the last
 * invocation of one of the rulbus_xxx() functions, followed by
 * a new-line. If the argument is NULL or an empty string only
 * the error message is printed. Returns the number of characters
 * printed.
 *----------------------------------------------------------------*/

int rulbus_perror( const char *s )
{
    if ( s != NULL && *s != '\0' )
        return fprintf( stderr, "%s: %s\n",
                        s, rulbus_errlist[ - rulbus_errno ] );

    return fprintf( stderr, "%s\n", rulbus_errlist[ - rulbus_errno ] );
}


/*---------------------------------------------------------------*
 * Returns a string with a short descriptive text of the error
 * encountered in the last invocation of one of the rulbus_xxx()
 * functions.
 *---------------------------------------------------------------*/

const char *rulbus_strerror( void )
{
    return rulbus_errlist[ - rulbus_errno ];
}


/*-------------------------------------------------------------------*
 * Function that must be called for a card before it can be used -
 * it expects the name of the card (as defined in the configuration
 * file) as the only argument and returns a handle for the card
 * that's going to be used for all further function calls for the
 * card.
 *-------------------------------------------------------------------*/

int rulbus_card_open( const char *name )
{
	int i;


	if ( ! rulbus_in_use )
		return rulbus_errno = RULBUS_NO_INIT;

	if ( name == NULL || *name == '\0' )
		return rulbus_errno = RULBUS_INV_ARG;

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( ! strcmp( name, rulbus_card[ i ].name ) )
			break;

	if ( i == rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_CRD;

	rulbus_card[ i ].in_use = SET;

	return i;
}


/*-------------------------------------------------------------------*
 * Function that can be used to "deactivate" a card
 *-------------------------------------------------------------------*/

int rulbus_card_close( int handle )
{
	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_HND;

	rulbus_card[ handle ].in_use = UNSET;

	return RULBUS_OK;
}


/*----------------------------------------------------------------------*
 * Function for sending a single byte to the card, expects the card
 * handle, an offset (relative to the base address of the card) and,
 * finally, the data byte to be send.
 *----------------------------------------------------------------------*/

int rulbus_write( int handle, unsigned char offset, unsigned char data )
{
	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_HND;

	if ( ! rulbus_card[ handle ].in_use )
		return rulbus_errno = RULBUS_CRD_NOP;

	if ( offset >= rulbus_card[ handle ].width )
		return rulbus_errno = RULBUS_INV_OFF;

	return rulbus_errno =
		rulbus_write_rack( rulbus_card[ handle ].rack,
						   rulbus_card[ handle ].addr + offset, data );
}


/*------------------------------------------------------------------------*
 * Function for reading a single byte from the card, expects the card
 * handle, an offset (relative to the base address of the card) and,
 * finally, the a pointer through which the data byte will be returned.
 *------------------------------------------------------------------------*/

int rulbus_read( int handle, unsigned char offset, unsigned char *data )
{
	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_HND;

	if ( ! rulbus_card[ handle ].in_use )
		return rulbus_errno = RULBUS_CRD_NOP;

	if ( offset >= rulbus_card[ handle ].width )
		return rulbus_errno = RULBUS_INV_OFF;

	return rulbus_errno =
		rulbus_read_rack( rulbus_card[ handle ].rack,
						  rulbus_card[ handle ].addr + offset, data );
}


/*-------------------------------------------------------------------*
 * Function to do the remaining checks on the data just read in from
 * the configuration file
 *-------------------------------------------------------------------*/

static int rulbus_check_config( void )
{
	int i, j;


	/* If no device file name was read use the default name */

	if ( rulbus_dev_file == NULL &&
		 ( rulbus_dev_file = strdup( RULBUS_DEFAULT_DEVICE_FILE ) ) == NULL )
		return rulbus_errno = RULBUS_NO_MEM;

	/* Check that there were cards listed */

	if ( rulbus_num_cards == 0 )
		return rulbus_errno = RULBUS_NO_CRD;

	/* Check that all cards have been assigned a name and set their widths,
	   i.e. how many 8-bit addresses are used by the cards, and, while were
	   at it, also check if the cards types make sense */

	for ( i = 0; i < rulbus_num_cards; i++ )
	{
		if ( rulbus_card[ i ].name == NULL )
			return rulbus_errno = RULBUS_CRD_NAM;

		switch ( rulbus_card[ i ].type )
		{
			case RB8509 :
				rulbus_card[ i ].width = RB8509_WIDTH;
				break;

			case RB8510 :
				rulbus_card[ i ].width = RB8510_WIDTH;
				break;

			case RB8514 :
				rulbus_card[ i ].width = RB8514_WIDTH;
				break;

			case RB8515 :
				rulbus_card[ i ].width = RB8515_WIDTH;
				break;

			default :
				return rulbus_errno = RULBUS_INV_TYP;
		}
	}

	/* Check that address ranges of cards on the same rack don't overlap */

	for ( i = 0; i < rulbus_num_cards - 1; i++ )
		for ( j = i + 1; i < rulbus_num_cards; j++ )
		{
			if ( rulbus_card[ i ].rack != rulbus_card[ j ].rack )
				continue;

			if ( rulbus_card[ i ].addr == rulbus_card[ j ].addr ||
				 ( rulbus_card[ i ].addr < rulbus_card[ i ].addr &&
				   rulbus_card[ i ].addr + rulbus_card[ i ].width >
				   rulbus_card[ j ].addr ) ||
				 ( rulbus_card[ j ].addr < rulbus_card[ j ].addr &&
				   rulbus_card[ j ].addr + rulbus_card[ j ].width >
				   rulbus_card[ i ].addr ) )
				return rulbus_errno = RULBUS_ADD_CFL;
		}

	return rulbus_errno = RULBUS_OK;
}


/*-------------------------------------------------------------------*
 * Function to get rid of all memory when either an fatal error was
 * detected or the RULBUS is "closed"
 *-------------------------------------------------------------------*/

static void rulbus_cleanup( void )
{
	int i;


	if ( rulbus_card )
	{
		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].name != NULL )
				free( rulbus_card[ i ].name );

		free( rulbus_card );
		rulbus_card = NULL;
		rulbus_num_cards = 0;
	}

	if ( rulbus_dev_file != NULL )
	{
		free( rulbus_dev_file );
		rulbus_dev_file = NULL;
	}
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

static int rulbus_write_rack( unsigned char rack, unsigned char addr,
							  unsigned char data )
{
	RULBUS_EPP_IOCTL_ARGS args = { rack, addr, data };

	if ( ioctl( fd, RULBUS_EPP_IOC_WRITE, &args ) < 0 )
		return rulbus_errno = RULBUS_WRT_ERR;

	return RULBUS_OK;
}


/*-------------------------------------------------------------------*
 *-------------------------------------------------------------------*/

static int rulbus_read_rack( unsigned char rack, unsigned char addr,
							 unsigned char *data )
{

	RULBUS_EPP_IOCTL_ARGS args = { rack, addr, 0 };

	if ( ioctl( fd, RULBUS_EPP_IOC_WRITE, &args ) < 0 )
		return rulbus_errno = RULBUS_RD_ERR;

	*data = args.data;
	return RULBUS_OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
