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
#include "rulbus_epp.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


RULBUS_CARD_LIST *rulbus_card = NULL;
int rulbus_num_cards = 0;
char *rulbus_dev_file = NULL;
int rulbus_errno = RULBUS_OK;


static const char *rulbus_errlist[ ] = {
	"Success",                          /* RULBUS_OK */
	"Running out of memory",            /* RULBUS_NO_MEMORY */
	"No permission to open configuration file",
										/* RULBUS_CONF_FILE_ACCESS */
	"Invalid name of configuration file or missing configuration file",
										/* RULBUS_CONF_FILE_NAME_INVALID */
    "Can't open configuration file",    /* RULBUS_CONF_FILE_OPEN_FAIL */
	"Syntax error in configuration file before line %d, column %d",
	                                    /* RULBUS_CF_SYNTAX_ERROR */
	"End of configuration file in comment",
										/* RULBUS_CF_EOF_IN_COMMENT */
	"More than one device file in configuration file before line %d, "
	"column %d",                        /* RULBUS_CF_DEV_FILE_DUPLICATE */
	"Missing rack name in configuration file before line %d, column %d",
										/* RULBUS_CF_NO_RACK_NAME */
	"More than one address for the same rack in configuration file before "
	"line %d, column %d",				/* RULBUS_CF_RACK_ADDR_DUPLICATE */
	"Invalid rack address in configuration file before line %d, column %d",
										/* RULBUS_CF_RACK_ADDR_INVALID */
	"Address conflict between racks in configuration file before line %d, "
	"column %d",                        /* RULBUS_CF_RACK_ADDR_CONFLICT */
	"Only one rack can be used when the default rack address must be used as "
	"detected in configuration file before line %d, column %d",
										/* RULBUS_CF_RACK_ADDR_DEF_DUPLICATE */
	"Name conflict  between racks in configuration file before line %d, "
	"column %d",                        /* RULBUS_CF_RACK_NAME_CONFLICT */
	"Unsupported card type in configuration file before line %d, column %d",
										/* RULBUS_CF_UNSUPPORTED_CARD_TYPE */
	"Identical names used for two cards in configuration file before line %d, "
	"column %d",                        /* RULBUS_CF_CARD_NAME_CONFLICT */
	"Invalid card address in configuration file before line %d, column %d",
										/* RULBUS_CF_CARD_ADDR_INVALID */
	"Identical addresses for different cards in same rack in in configuration "
	"file before line %d",              /* RULBUS_CF_CARD_ADDR_CONFLICT  */
	"Different address already specified for card in configuration file "
	"before line %d, column %d",        /* RULBUS_CF_CARD_ADDR_DUPLICATE */
	"More than one card of same type without specified address in same rack "
	"configuration file before line %d, column %d",
										/* RULBUS_CF_CARD_ADDR_DEF_CONFLICT */
	"Address ranges of cards overlap in configuration file before line %d, "
	"column %d",                        /* RULBUS_CF_CARD_ADDR_OVERLAP */
	"Non-zero address specified for card of type \"rb_generic\" in "
	"configuration file before line %d, column %d",
										/* RULBUS_CF_CARD_ADDR_GENERIC */
	"Specified property not applicable for card of this type in configuration "
	"file before line %d, column %d",   /* RULBUS_CF_CARD_PROPERTY_INVALID */
	"Different number of channels already specified for card in configuration "
	"file before line %d, column %d",   /* RULBUS_CF_DUPLICATE_NUM_CHANNELS */
	"Invalid number of channels specified in configuration file before "
	"line %d, column %d",      			/* RULBUS_CF_INVALID_NUM_CHANNELS */
	"Different 'volt_per_bit' values already specified for card in "
	"configuration file before line %d, column %d",
										/* RULBUS_CF_VPB_DUPLICATE */
	"Invalid 'volt_per_bit' value specified in configuration file before line "
	"%d, column %d",               		/* RULBUS_CF_INVALID_VPB */
	"Different settings for 'bipolar' property specified for card in "
	"configuration file before line %d, column %d",
										/* RULBUS_CF_BIPLOAR_DUPLICATE */
	"Different settings for 'has_ext_trigger' proper specified for card in "
	"configuration file before line %d, column %d",
										/* RULBUS_CF_EXT_TRIGGER_DUPLICATE */
	"Different 'intr_delay' value already pecified for card in configuration "
	"file before line %d, column %d",   /* RULBUS_CF_INTR_DELAY_DUPLICATE */
	"Invalid 'intr_delay' value specified in configuration file before line "
	"%d, column %d",               		/* RULBUS_CF_INTR_DELAY_INVALID */
	"Not permission to open device file",
										/* RULBUS_DEV_FILE_ACCESS */
	"Invalid device file name or missing device file",
										/* RULBUS_DEV_FILE_NAME_INVALID */
	"Can't open device file",           /* RULBUS_DEV_FILE_OPEN_FAIL */
	"Device doesn't exit or driver not loaded",
										/* RULBUS_DEV_NO_DEVICE */
	"Missing intialization of Rulbus library",
										/* RULBUS_NO_INITIALIZATION */
	"Invalid function argument",        /* RULBUS_INVALID_ARGUMENT */
	"No such card with that name",      /* RULBUS_INVALID_CARD_NAME */
	"Invalid card handle",              /* RULBUS_INVALID_CARD_HANDLE */
	"Card has not been opened",         /* RULBUS_CARD_NOT_OPEN */
	"Invalid card address offset",      /* RULBUS_INVALID_CARD_OFFSET */
	"Rulbus write error",               /* RULBUS_WRITE_ERROR */
	"Rulbus read error",                /* RULBUS_READ_ERROR  */
	"Card is busy",                     /* RULBUS_CARD_IS_BUSY */
	"Voltage out of range",             /* RULBUS_INVALID_VOLTAGE */
	"Rulbus timeout",                   /* RULBUS_TIME_OUT */
	"No clock frequency has been set for delay card",
										/* RULBUS_NO_CLOCK_FREQ */
	"Card does not have external trigger capabilities"
										/* RULBUS_NO_EXT_TRIGGER */
};


static char rulbus_err_str[ 256 ];

static const int rulbus_nerr =
                ( int ) ( sizeof rulbus_errlist / sizeof rulbus_errlist[ 0 ] );

static int rulbus_in_use = UNSET;
static int fd;


static int rulbus_check_config( void );
static void rulbus_cleanup( void );
static int rulbus_write_rack( unsigned char rack, unsigned char addr,
							  unsigned char *data, size_t len );
static int rulbus_read_rack( unsigned char rack, unsigned char addr,
							 unsigned char *data, size_t len );

extern int rulbus_parse( void );
extern void rulbus_parser_init( void );
extern void rulbus_parser_clean_up( void );


typedef struct RULBUS_CARD_HANDLER RULBUS_CARD_HANDLER;

struct RULBUS_CARD_HANDLER {
	int card_type;
	int ( *init ) ( void );       /* module initialization function */
	void ( *exit ) ( void );      /* module cleanup function */
	int ( *card_init ) ( int );   /* card initialization function */
	void ( *card_exit ) ( int );  /* card cleanup function */
	int is_init;                  /* module initialization flag */
};

static RULBUS_CARD_HANDLER rulbus_card_handler[ ] =
{
	{                /* Generic card */
		RB_GENERIC,
		rulbus_generic_init,
		rulbus_generic_exit,
		rulbus_generic_card_init,
		rulbus_generic_card_exit,
		UNSET
	},
				
	{                /* RB8509 12-bit ADC card */
		RB8509_ADC12,
		rulbus_rb8509_adc12_init,
		rulbus_rb8509_adc12_exit,
		rulbus_rb8509_adc12_card_init,
		rulbus_rb8509_adc12_card_exit,
		UNSET
	},
	{                /* RB8510 12 bit DAC card */
		RB8510_DAC12,
		rulbus_rb8510_dac12_init,
		rulbus_rb8510_dac12_exit,
		rulbus_rb8510_dac12_card_init,
		rulbus_rb8510_dac12_card_exit,
		UNSET
	},
	{                /* RB8514 delay card */
		RB8514_DELAY,
		rulbus_rb8514_delay_init,
		rulbus_rb8514_delay_exit,
		rulbus_rb8514_delay_card_init,
		rulbus_rb8514_delay_card_exit,
		UNSET
	},
	{                /* RB8515 clock card */
		RB8515_CLOCK,
		rulbus_rb8515_clock_init,
		rulbus_rb8515_clock_exit,
		rulbus_rb8515_clock_card_init,
		rulbus_rb8515_clock_card_exit,
		UNSET
	},
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
	extern FILE *rulbus_in;             /* defined in parser.y */
	int fd_flags;
	int retval;
	int i;


	/* Don't try to reinitialize */

	if ( rulbus_in_use )
		return rulbus_errno = RULBUS_OK;

	/* Figure out the name of the configuration file and try to open it */

	if ( ( config_name = getenv( "RULBUS_CONFIG_FILE" ) ) == NULL )
		config_name = RULBUS_DEFAULT_CONFIG_FILE;

	if ( ( rulbus_in = fopen( config_name, "r" ) ) == NULL )
		switch ( errno )
		{
			case ENOMEM :
				return rulbus_errno = RULBUS_NO_MEMORY;

			case EACCES :
				return rulbus_errno = RULBUS_CONF_FILE_ACCESS;

			case ENOENT : case ENOTDIR : case ENAMETOOLONG : case ELOOP :
				return rulbus_errno = RULBUS_CONF_FILE_NAME_INVALID;

			default :
				return rulbus_errno = RULBUS_CONF_FILE_OPEN_FAIL;
		}

	/* Parse the configuration file */

	rulbus_parser_init( );
    retval = rulbus_parse( );
	rulbus_parser_clean_up( );

	fclose( rulbus_in );
	rulbus_parser_clean_up( );

	if ( retval != RULBUS_OK )
	{
		rulbus_cleanup( );
		return rulbus_errno = retval;
    }

	/* Check that the data from the configuration file were reasonable */

	if ( ( retval = rulbus_check_config( ) ) != RULBUS_OK )
	{
		rulbus_cleanup( );
		return rulbus_errno = retval;
	}

	/* Try to open the device file */

	if ( ( fd = open( rulbus_dev_file, O_RDWR | O_EXCL | O_NONBLOCK) ) < 0 )
	{
		int stored_errno = errno;

		rulbus_cleanup( );

		switch ( stored_errno )
		{
			case ENOMEM :
				return rulbus_errno = RULBUS_NO_MEMORY;

			case EACCES :
				return rulbus_errno = RULBUS_DEV_FILE_ACCESS;

			case ENOENT : case ENOTDIR : case ENAMETOOLONG : case ELOOP :
				return rulbus_errno = RULBUS_DEV_FILE_NAME_INVALID;

			case ENODEV : case ENXIO :
				return rulbus_errno = RULBUS_DEV_NO_DEVICE;

			default :
				return rulbus_errno = RULBUS_DEV_FILE_OPEN_FAIL;
		}
	}

    /* Set the close-on-exec flag for the device file descriptor */

    if ( ( fd_flags = fcntl( fd, F_GETFD, 0 ) ) >= 0 )
		fcntl( fd, F_SETFD, fd_flags | FD_CLOEXEC );

	rulbus_in_use = SET;

	/* Call initialization functions of all card drivers that may be needed */

	for ( i = 0; i < rulbus_num_card_handlers; i++ )
	{
		if ( rulbus_card_handler[ i ].init &&
			 ( retval = rulbus_card_handler[ i ].init( ) ) != RULBUS_OK )
		{
			for ( --i; i >= 0; i-- )
				if ( rulbus_card_handler[ i ].exit )
					rulbus_card_handler[ i ].exit( );
			rulbus_close( );

			return rulbus_errno = retval;
		}
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
	extern int rulbus_lineno;
	extern int rulbus_column;
	char *p = ( char * ) rulbus_errlist[ - rulbus_errno ];
	int count = 0;


    if ( s != NULL && *s != '\0' )
		count +=fprintf( stderr, "%s: ", s );

	if ( strstr( p, "%d" ) )
        count += fprintf( stderr, p,
						  rulbus_lineno, rulbus_column );
	else
		count += fprintf( stderr, "%s", p );

	return count += fprintf( stderr, "\n" );
}


/*---------------------------------------------------------------*
 * Returns a string with a short descriptive text of the error
 * encountered in the last invocation of one of the rulbus_xxx()
 * functions.
 *---------------------------------------------------------------*/

const char *rulbus_strerror( void )
{
	extern int rulbus_lineno;
	extern int rulbus_column;


	if ( ! strstr( rulbus_errlist[ - rulbus_errno ], "%%d" ) )
	{
		sprintf( rulbus_err_str, rulbus_errlist[ - rulbus_errno ],
				 rulbus_lineno, rulbus_column );
		return rulbus_err_str;
	}

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
		rulbus_errno = RULBUS_NO_INITIALIZATION;

	if ( name == NULL || *name == '\0' )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( ! strcmp( name, rulbus_card[ i ].name ) )
			break;

	if ( i == rulbus_num_cards )
		return rulbus_errno = RULBUS_INVALID_CARD_NAME;

	/* Call the type specific funtion for initialization of the card */

	if ( ! rulbus_card[ i ].in_use )
	{
		rulbus_card[ i ].in_use = SET;
		if ( rulbus_card[ i ].handler->card_init &&
			 ( retval = rulbus_card[ i ].handler->card_init( i ) ) < 0 )
		{
			rulbus_card[ i ].in_use = UNSET;
			return rulbus_errno = retval;
		}
	}

	return i;
}


/*-------------------------------*
 * Function to deactivate a card
 *-------------------------------*/

int rulbus_card_close( int handle )
{
	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

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
		return rulbus_errno = RULBUS_NO_INITIALIZATION;

	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( ! rulbus_card[ handle ].in_use )
		return rulbus_errno = RULBUS_CARD_NOT_OPEN;

	if ( offset >= rulbus_card[ handle ].width )
		return rulbus_errno = RULBUS_INVALID_CARD_OFFSET;

	if ( data == NULL || len == 0 )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	retval = rulbus_write_rack( rulbus_card[ handle ].rack,
								rulbus_card[ handle ].addr + offset,
								data, len );

	if ( retval <= 0 )
		return rulbus_errno = retval;

	rulbus_errno = RULBUS_OK;
	return retval;               /* return number of bytes written */
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
		return rulbus_errno = RULBUS_NO_INITIALIZATION;

	if ( handle < 0 || handle >= rulbus_num_cards )
		return rulbus_errno = RULBUS_INVALID_CARD_HANDLE;

	if ( ! rulbus_card[ handle ].in_use )
		return rulbus_errno = RULBUS_CARD_NOT_OPEN;

	if ( offset >= rulbus_card[ handle ].width )
		return rulbus_errno = RULBUS_INVALID_CARD_OFFSET;

	if ( data == NULL || len == 0 )
		return rulbus_errno = RULBUS_INVALID_ARGUMENT;

	retval = rulbus_read_rack( rulbus_card[ handle ].rack,
							   rulbus_card[ handle ].addr + offset,
							   data, len );

	if ( retval <= 0 )
		return rulbus_errno = retval;

	rulbus_errno = RULBUS_OK;
	return retval;                    /* return number of bytes read */
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
		return RULBUS_NO_MEMORY;

	/* Return if there are no cards */

	if ( rulbus_num_cards == 0 )
		return RULBUS_OK;

	/* Figure out which handler functions to use for the card */

	for ( i = 0; i < rulbus_num_cards; i++ )
	{
		for ( j = 0; j < rulbus_num_card_handlers; j++ )
			if ( rulbus_card[ i ].type == rulbus_card_handler[ j ].card_type )
				break;

		rulbus_card[ i ].handler = rulbus_card_handler + j;
		rulbus_card[ i ].in_use = UNSET;
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
	int retval;


	if ( ( retval = ioctl( fd, RULBUS_EPP_IOC_WRITE, &args ) ) <= 0 )
		switch ( errno )
		{
			case ENOMEM :
				return RULBUS_NO_MEMORY;

			case EIO :
				return RULBUS_TIME_OUT;

			default :                   /* catch all for "impossible" errors */
				return RULBUS_WRITE_ERROR;
		}

	return retval;
}


/*------------------------------------------------------------------*
 * Function for reading a number of bytes from a certain address at
 * one of the racks.
 *------------------------------------------------------------------*/

static int rulbus_read_rack( unsigned char rack, unsigned char addr,
							 unsigned char *data, size_t len )
{
	RULBUS_EPP_IOCTL_ARGS args = { rack, addr, 0, data, len };
	int retval;


	if ( ( retval = ioctl( fd, RULBUS_EPP_IOC_READ, &args ) ) <= 0 )
		switch ( errno )
		{
			case ENOMEM :
				return RULBUS_NO_MEMORY;

			case EIO :
				return RULBUS_TIME_OUT;

			default :                   /* catch all for "impossible" errors */
				return RULBUS_READ_ERROR;
		}

	return retval;
}


/*--------------------------------------------------------------*
 * Function returns some informations about the cards as far as
 * specified by the configuration file
 *--------------------------------------------------------------*/

int rulbus_get_card_info( const char *card_name, RULBUS_CARD_INFO *card_info )
{
	const char *config_name;
	extern FILE *rulbus_in;             /* defined in parser.y */
	int retval;
	int i;


	if ( card_name == NULL || card_info == NULL )
		return RULBUS_INVALID_ARGUMENT;

	if ( ! rulbus_in_use )
	{
		/* Figure out the name of the configuration file and try to open it */

		if ( ( config_name = getenv( "RULBUS_CONFIG_FILE" ) ) == NULL )
			config_name = RULBUS_DEFAULT_CONFIG_FILE;

		if ( ( rulbus_in = fopen( config_name, "r" ) ) == NULL )
			switch ( errno )
			{
				case ENOMEM :
					return rulbus_errno = RULBUS_NO_MEMORY;

				case EACCES :
					return rulbus_errno = RULBUS_CONF_FILE_ACCESS;

				case ENOENT : case ENOTDIR : case ENAMETOOLONG : case ELOOP :
					return rulbus_errno = RULBUS_CONF_FILE_NAME_INVALID;

				default :
					return rulbus_errno = RULBUS_CONF_FILE_OPEN_FAIL;
			}

		/* Parse the configuration file */

		rulbus_parser_init( );
		retval = rulbus_parse( );
		rulbus_parser_clean_up( );
		fclose( rulbus_in );

		if ( retval != RULBUS_OK ||
			 ( retval = rulbus_check_config( ) ) != RULBUS_OK )
		{
			rulbus_cleanup( );
			return rulbus_errno = retval;
		}
	}

	for ( i = 0; i < rulbus_num_cards; i++ )
		if ( ! strcmp( card_name, rulbus_card[ i ].name ) )
			break;

	if ( i == rulbus_num_cards )
	{
		rulbus_cleanup( );
		return rulbus_errno = RULBUS_INVALID_CARD_NAME;
	}

	card_info->type = rulbus_card[ i ].type;
	card_info->num_channels = rulbus_card[ i ].num_channels;
	card_info->volt_per_bit = rulbus_card[ i ].vpb;
	card_info->bipolar = rulbus_card[ i ].bipolar;
	card_info->has_ext_trigger = rulbus_card[ i ].has_ext_trigger;
	card_info->intr_delay = rulbus_card[ i ].intr_delay;

	rulbus_cleanup( );
	return RULBUS_OK;	
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
