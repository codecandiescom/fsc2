//Olof Grund
//2017-01-31
//FSC2 Module

/* Include configuration information for the device */

#include "mc_1024ls.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#include <hidapi/hidapi.h>
#include <stdint.h>
#include "fsc2_module.h"


#define MCC_VID         0x09db   // Vendor ID for Measurement Computing
#define USB1024LS_PID   0x0076   // Product ID

#define DIO_PORTA       0x01
#define DOUT            0x01     // Write digital port
#define DIO_DIR_OUT     0x00
#define DCONFIG         0x0d     // Configure digital port


/* Structure with information about state of the device */

static struct
{
	hid_device * hid;
} mc_1024ls;


/* Hook functions */

int mc_1024ls_init_hook( void );
int mc_1024ls_test_hook( void );
int mc_1024ls_exp_hook( void );
int mc_1024ls_end_of_exp_hook( void );


/* EDL functions */

Var_T * dio_name( Var_T * v );
Var_T * dio_pulse( Var_T * v );
Var_T * dio_value( Var_T * v );


/*-------------------------------------------------------------*
 * Hook function that's run when the module gets loaded - just
 * set the variable that tells that the libusb must be initia-
 * lized and set up a few internal variables.
 *-------------------------------------------------------------*/

int
mc_1024ls_init_hook( void )
{
    return 1;
}


/*-------------------------------------------------------------*
 * Hook function run at the start of a test. Save the internal
 * data that might contain information about things that need
 * to be set up at the start of the experiment
 *-------------------------------------------------------------*/

int
mc_1024ls_test_hook( void )
{
    return 1;
}


/*------------------------------------------------------------*
 * Hook function run at the start of the experiment. Needs to
 * initialize communication with the device and set it up.
 *------------------------------------------------------------*/

int
mc_1024ls_exp_hook( void )
{
	struct
	{
		uint8_t report_id;
		uint8_t cmd;
		uint8_t port;
		uint8_t direction;
		uint8_t pad[ 5 ];
	} report = { 0, DCONFIG, DIO_PORTA, DIO_DIR_OUT, { 0, 0, 0, 0, 0 } };
 
	// Open hid connection
	
    raise_permissions( );
	if ( ! ( mc_1024ls.hid = hid_open( MCC_VID, USB1024LS_PID, NULL ) ) )
	{
        lower_permissions( );
		print( FATAL, "Failed to open connection to device %s %d.\n",
               strerror( errno ), errno );
		return 0;
    }
    lower_permissions( );
	fsc2_usleep( 100000, UNSET );

	// Configure ports

    raise_permissions( );
	if ( hid_write( mc_1024ls.hid, ( const unsigned char * ) &report,
					sizeof report ) == -1 )
	{
        lower_permissions( );
		print( FATAL, "Failed to write to device\n" );
		return 0;
	}
    lower_permissions( );
	fsc2_usleep( 100000, UNSET );

    return 1;
}


/*---------------------------------------------------------*
 * Hook function run at the end of the experiment. Closes
 * the connection to the device.
 *---------------------------------------------------------*/

int
mc_1024ls_end_of_exp_hook( void )
{
	if ( mc_1024ls.hid )
	{
        raise_permissions( );
		hid_close( mc_1024ls.hid );
        lower_permissions( );
		mc_1024ls.hid = NULL;
	}

	return 1;
}


/*----------------------------------------------------------------*
 * Function returns a string variable with the name of the device
 *----------------------------------------------------------------*/

Var_T *
dio_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T *
dio_pulse( Var_T * v )
{
	uint8_t cmd[ 8 ];
    double pd = get_double( v, "pulse duration" );

    /* Check the only (and required) argument, the pulse duration. */

    if ( pd <= 0.0 )
    {
        print( FATAL, "Invalid negative or zero pulse duration.\n" );
        THROW( EXCEPTION );
    }
    else if ( pd > 1.0e-6 * ULONG_MAX )
    {
        print( FATAL, "Pulse duration too long.\n" );
        THROW( EXCEPTION );
    }

	/* Nothing further to be done during test run */

	if ( FSC2_MODE == TEST )
		return vars_push( FLOAT_VAR, pd );

	// Switch output pin on...
  
	cmd[ 0 ] = 0;       // Report ID is always 0
	cmd[ 1 ] = DOUT;
	cmd[ 2 ] = DIO_PORTA;
	cmd[ 3 ] = 1;

    raise_permissions( );
	if ( hid_write( mc_1024ls.hid, cmd, sizeof cmd ) == -1 )
	{
        lower_permissions( );
		print( FATAL, "Failed to write to device\n" );
		THROW( EXCEPTION );
	}
    lower_permissions( );

    fsc2_usleep( lrnd( 1.0e6 * pd ), UNSET );

	// ...and off again, thus ending the pulse

	cmd[ 3 ] = 0;
	
    raise_permissions( );
	if ( hid_write( mc_1024ls.hid, cmd, sizeof cmd ) == -1 )
	{
        lower_permissions( );
		print( FATAL, "Failed to write to device\n" );
		THROW( EXCEPTION );
	}
    lower_permissions( );

    /* Return the pulse duration */

    return vars_push( FLOAT_VAR, pd );
}

Var_T *
dio_value( Var_T * v )
{
    uint8_t cmd[ 8 ];
    bool state = get_boolean( v );

    too_many_arguments( v );
    
	/* Lust retiurn new state during test run */

	if ( FSC2_MODE == TEST )
		return vars_push( INT_VAR, ( long int ) state );

	/* Switch output pin on or off */
  
	cmd[ 0 ] = 0;          // Report ID is always 0
	cmd[ 1 ] = DOUT;
	cmd[ 2 ] = DIO_PORTA;
	cmd[ 3 ] = state;

    raise_permissions( );
	if ( hid_write( mc_1024ls.hid, cmd, sizeof cmd ) == -1 )
	{
        lower_permissions( );
		print( FATAL, "Failed to write to device\n" );
		THROW( EXCEPTION );
	}
    lower_permissions( );

    /* Return the new state */

    return vars_push( FLOAT_VAR, ( long int ) state );
}

/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
