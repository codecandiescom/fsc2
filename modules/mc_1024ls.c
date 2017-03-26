//Olof Grund
//2017-01-31
//FSC2 Module

/* Include configuration information for the device */

#include "mc_1024ls.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


#include "fsc2_module.h"
#include <libusb-1.0/libusb.h>
#include <hidapi/hidapi.h>


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

Var_T * dio_pulse( Var_T * v );


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
	
	if ( ! ( mc_1024ls.hid = hid_open( MCC_VID, USB1024LS_PID, NULL ) ) )
	{
		print( FATAL, "Failed to open connection to device.\n" );
		return 0;
    }
	fsc2_usleep( 100000, UNSET );

	// Configure ports

	if ( hid_write( mc_1024ls.hid, ( const unsigned char * ) &report,
					sizeof report ) == -1 )
	{
		print( FATAL, "Failed to write to device\n" );
		return 0;
	}
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
		hid_close( mc_1024ls.hid );
		mc_1024ls.hid = NULL;
	}

	return 1;
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
		return vars_push( INT_VAR, 1);

	// Switch output pin on...
	// usbDOut_USB1024LS(hid, DIO_PORTA, 1);
  
	cmd[ 0 ] = 0;       // Report ID is always 0
	cmd[ 1 ] = DOUT;
	cmd[ 2 ] = DIO_PORTA;
	cmd[ 3 ] = 1;

	if ( hid_write( mc_1024ls.hid, cmd, sizeof cmd ) == -1 )
	{
		print( FATAL, "Failed to write to device\n" );
		THROW( EXCEPTION );
	}

    fsc2_usleep( lrnd( 1.0e6 * pd ), UNSET );

	// ...and off again, thus ending the pulse
	// usbDOut_USB1024LS(hid, DIO_PORTA, 0 );

	cmd[ 3 ] = 0;
	
	if ( hid_write( mc_1024ls.hid, cmd, sizeof cmd ) == -1 )
	{
		print( FATAL, "Failed to write to device\n" );
		THROW( EXCEPTION );
	}

    /* Return the pulse duration */

    return vars_push( FLOAT_VAR, pd );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
