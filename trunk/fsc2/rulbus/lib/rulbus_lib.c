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
#include "rulbus_epp.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


RULBUS_CARD_LIST *rulbus_card = NULL;
int rulbus_num_cards = 0;
char *rulbus_dev_file = NULL;
int rulbus_errno = RULBUS_OK;


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
	"Duplicate card name in configuration file",       /* RULBUS_NAM_DUP */
	"More than one name for a card",                   /* RULBUS_CRD_NAM */
	"More than one address for a card",                /* RULBUS_ADD_DUP */
	"More than one type for a card",                   /* RULBUS_TYP_DUP */
	"Too many cards for single rack",                  /* RULBUS_CRD_CNT */
	"No cards found in configuration file",            /* RULBUS_NO_CRD  */
	"Missing card name in configuration file",         /* RULBUS_CRD_NAM */
	"Invalid card type in configuration file",         /* RULBUS_INV_TYP */
	"Address confict for cards in same rack",          /* RULBUS_ADD_CFL */
	"Missing modules for card",                        /* RULBUS_TYP_HND */
	"Missing intialization of library",                /* RULBUS_NO_INIT */
	"Invalid function argument",                       /* RULBUS_INV_ARG */
	"No such card",                                    /* RULBUS_INV_CRD */
	"Invalid card handle",                             /* RULBUS_INV_HND */
	"Card has not been opened",                        /* RULBUS_CRD_NOP */
	"Invalid card address offset",                     /* RULBUS_INV_OFF */
	"Write error"                                      /* RULBUS_WRT_ERR */
	"Read error"                                       /* RULBUS_RD_ERR  */
	"Card is busy",                                    /* RULBUS_CRD_BSY */
	"Invalid delay length",                            /* RULBUS_INV_DEL */
};

static const int rulbus_nerr =
                ( int ) ( sizeof rulbus_errlist / sizeof rulbus_errlist[ 0 ] );

static bool rulbus_in_use = UNSET;
static int fd;


static int rulbus_check_config( void );
static void rulbus_cleanup( void );
static int rulbus_write_rack( unsigned char rack, unsigned char addr,
							  unsigned char *data, size_t len );
static int rulbus_read_rack( unsigned char rack, unsigned char addr,
							 unsigned char *data, size_t len );

extern int yyparse( void );


typedef struct RULBUS_CARD_HANDLER RULBUS_CARD_HANDLER;

struct RULBUS_CARD_HANDLER {
	int card_type;
	int ( *init ) ( void );       /* module initialization function */
	void ( *exit ) ( void );      /* module cleanup function */
	int ( *card_init ) ( int );   /* card initialization function */
	void ( *card_exit ) ( int );  /* card cleanup function */
	bool is_init;                 /* module initialization flag */
};

RULBUS_CARD_HANDLER rulbus_card_handler[ ] =
		{
			{                /* RB8514 delay card */
				RB8514,
				rulbus_delay_init,
				rulbus_delay_exit,
				rulbus_delay_card_init,
				rulbus_delay_card_exit,
				UNSET
			},
#if 0
			{                /* RB8515 clock card */
				RB8514,
				rulbus_clock_init,
				rulbus_clock_exit,
				rulbus_clock_card_init,
				rulbus_clock_card_exit,
				UNSET
			},
#endif
		};

static const int rulbus_num_card_handlers = 
	  ( int ) ( sizeof rulbus_card_handler / sizeof rulbus_card_handler[ 0 ] );


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
	int i;


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

	/* Call initialization functions of all card drivers that may be needed */

	for ( i = 0; i < rulbus_num_card_handlers; i++ )
		if ( rulbus_card_handler[ i ].init &&
			 ( retval = rulbus_card_handler[ i ].init( ) ) < 0 )
		{
			for ( --i; i >= 0; i-- )
				if ( rulbus_card_handler[ i ].exit )
					rulbus_card_handler[ i ].exit( );
			rulbus_close( );

			return rulbus_errno = retval;
		}

	return rulbus_errno = RULBUS_OK;
}


/*-----------------------------------*
 * Function to "close" to the RULBUS
 *-----------------------------------*/

void rulbus_close( void )
{
	int i;


	if ( rulbus_in_use )
	{
		/* Deactivate all cards, we can't be sure it already has been done */

		for ( i = 0; i < rulbus_num_cards; i++ )
			if ( rulbus_card[ i ].in_use )
				rulbus_card_close( i );

		/* Call the deactivation function for all modules */

		for ( i = 0; i < rulbus_num_card_handlers; i++ )
			if ( rulbus_card_handler[ i ].exit )
				rulbus_card_handler[ i ].exit( );

		close( fd );
		rulbus_cleanup( );
		rulbus_in_use = UNSET;
	}

	rulbus_errno = RULBUS_OK;
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
	if ( ! rulbus_in_use )
		rulbus_errno = RULBUS_NO_INIT;

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
	if ( ! rulbus_in_use )
		return "";

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
	int retval;


	if ( ! rulbus_in_use )
		rulbus_errno = RULBUS_NO_INIT;

	if ( name == NULL || *name == '\0' )
		return rulbus_errno = RULBUS_INV_ARG;

	if ( ! rulbus_in_use )
		return rulbus_errno = RULBUS_NO_INIT;

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( ! strcmp( name, rulbus_card[ i ].name ) )
			break;

	if ( i == rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_CRD;

	/* Call the type specific funtion for initialization of the card */

	if ( ! rulbus_card[ i ].in_use )
	{
		if ( rulbus_card[ i ].handler->card_init &&
			 ( retval = rulbus_card[ i ].handler->card_init( i ) ) < 0 )
			return retval;
		rulbus_card[ i ].in_use = SET;
	}

	return i;
}


/*-------------------------------*
 * Function to deactivate a card
 *-------------------------------*/

int rulbus_card_close( int handle )
{
	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_HND;

	/* Call the type specific funtion for deactivating the card */

	if ( rulbus_card[ handle ].in_use )
	{
		if ( rulbus_card[ handle ].handler->card_exit )
			rulbus_card[ handle ].handler->card_exit( handle );
		rulbus_card[ handle ].in_use = UNSET;
	}

	return rulbus_errno = RULBUS_OK;
}


/*---------------------------------------------------------------*
 * Function for sending a 'len' bytes to a card, expects a card
 * handle, an offset (relative to the base address of the card),
 * a pointer to a buffer with the data to be written and the
 * number of bytes to be written. On success it returns the
 * number of bytes that were written, otherwise a (negative)
 * value indicating an error.
 *---------------------------------------------------------------*/

int rulbus_write( int handle, unsigned char offset, unsigned char *data,
				  size_t len)
{
	int retval;


	if ( ! rulbus_in_use )
		rulbus_errno = RULBUS_NO_INIT;

	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_HND;

	if ( ! rulbus_card[ handle ].in_use )
		return rulbus_errno = RULBUS_CRD_NOP;

	if ( offset >= rulbus_card[ handle ].width )
		return rulbus_errno = RULBUS_INV_OFF;

	if ( data == NULL || len == 0 )
		return rulbus_errno RULBUS_INV_ARG;

	retval = rulbus_write_rack( rulbus_card[ handle ].rack,
								rulbus_card[ handle ].addr + offset,
								data, len );

	if ( retval <= 0 )
		return rulbus_errno = RULBUS_WRT_ERR;

	rulbus_errno = RULBUS_OK;
	return retval;
}


/*------------------------------------------------------------------*
 * Function for reading a 'len' byte a the card. It expects a card
 * handle, an offset (relative to the base address of the card), a
 * pointer to a buffer to which the data byte will be written and
 * the maximum number of bytes to be read. On succes it returns
 * the number of bytes that were read, otherwise a (negative) value
 * indicating an error.
 *------------------------------------------------------------------*/

int rulbus_read( int handle, unsigned char offset, unsigned char *data,
				 size_t len )
{
	int retval;


	if ( ! rulbus_in_use )
		rulbus_errno = RULBUS_NO_INIT;

	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INV_HND;

	if ( ! rulbus_card[ handle ].in_use )
		return rulbus_errno = RULBUS_CRD_NOP;

	if ( offset >= rulbus_card[ handle ].width )
		return rulbus_errno = RULBUS_INV_OFF;

	if ( data == NULL || len == 0 )
		return rulbus_errno RULBUS_INV_ARG;

	retval = rulbus_read_rack( rulbus_card[ handle ].rack,
							   rulbus_card[ handle ].addr + offset,
							   data, len );

	if ( retval <= 0 )
		return rulbus_errno = RULBUS_RD_ERR;

	rulbus_errno = RULBUS_OK;
	return retval;
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
		return RULBUS_NO_MEM;

	/* Check that there were cards listed */

	if ( rulbus_num_cards == 0 )
		return RULBUS_NO_CRD;

	/* Check that all cards have been assigned a name and set their widths,
	   i.e. how many 8-bit addresses are used by the cards, and, while were
	   at it, also check if the cards types make sense and there's a
	   registered handler for the card type (which then is assigned to the
	   card)  */

	for ( i = 0; i < rulbus_num_cards; i++ )
	{
		if ( rulbus_card[ i ].name == NULL )
			return RULBUS_CRD_NAM;

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
				return RULBUS_INV_TYP;
		}

		for ( j = 0; j < rulbus_num_card_handlers; j++ )
			if ( rulbus_card[ i ].type == rulbus_card_handler[ j ].card_type )
				break;

		if ( j == rulbus_num_card_handlers )
			return RULBUS_TYP_HND;

		rulbus_card[ i ].handler = rulbus_card_handler + j;
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
				return RULBUS_ADD_CFL;
		}

	return RULBUS_OK;
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


/*----------------------------------------------------------------*
 * Function for writing a number of bytes to a certain address at
 * one of the racks.
 *----------------------------------------------------------------*/

static int rulbus_write_rack( unsigned char rack, unsigned char addr,
							  unsigned char *data, size_t len )
{
	RULBUS_EPP_IOCTL_ARGS args = { rack, addr, *data, data, len };

	return ioctl( fd, RULBUS_EPP_IOC_WRITE, &args );
}


/*------------------------------------------------------------------*
 * Function for reading a number of bytes from a certain address at
 * one of the racks (with special handling for the case of a single
 * byte to be read).
 *------------------------------------------------------------------*/

static int rulbus_read_rack( unsigned char rack, unsigned char addr,
							 unsigned char *data, size_t len )
{
	int retval;
	RULBUS_EPP_IOCTL_ARGS args = { rack, addr, 0, data, len };

	if ( ( retval = ioctl( fd, RULBUS_EPP_IOC_WRITE, &args ) ) <= 0 )
		return retval;

	if ( len == 1 )
		*data = byte;

	return retval;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */