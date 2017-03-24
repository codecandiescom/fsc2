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
#define USB1024LS_PID   0x0076
#define DIO_PORTA       0x01
#define DOUT            0x01     // Write digital port
#define DIO_DIR_OUT     0x00
#define DCONFIG         0x0D     // Configure digital port


/* Hook functions */

int mc_1024ls_init_hook( void );
int mc_1024ls_test_hook( void );
int mc_1024ls_exp_hook( void );
int mc_1024ls_end_of_exp_hook( void );


/* Meilhaus function */

Var_T * dio_pulse( Var_T * v  UNUSED_ARG );


/*-------------------------------------------------------------*
 * Hook function that's run when the module gets loaded - just
 * set the variable that tells that the libusb must be initia-
 * lized and set up a few internal variables.
 *-------------------------------------------------------------*/

int mc_1024ls_init_hook( void )
{
    Need_USB = SET;

    return 1;
}


/*-------------------------------------------------------------*
 * Hook function run at the start of a test. Save the internal
 * data that might contain information about things that need
 * to be set up at the start of the experiment
 *-------------------------------------------------------------*/

int mc_1024ls_test_hook( void )
{
    return 1;
}


/*------------------------------------------------------------*
 * Hook function run at the start of the experiment. Needs to
 * initialize communication with the device and set it up.
 *------------------------------------------------------------*/

int mc_1024ls_exp_hook( void )
{
    return 1;
}


/*---------------------------------------------------------*
 * Hook function run at the end of the experiment. Closes
 * the connection to the device.
 *---------------------------------------------------------*/

int mc_1024ls_end_of_exp_hook()
{
    return 1;
}


/*---------------------------------------------------------*
 *---------------------------------------------------------*/

Var_T * dio_pulse( Var_T * v  UNUSED_ARG )
{
	hid_device * hid = NULL;
	struct {
		uint8_t report_id;
		uint8_t cmd;
		uint8_t port;
		uint8_t direction;
		uint8_t pad[ 5 ];
	} report;
	uint8_t cmd[ 8 ];
 
    if ( FSC2_MODE == TEST )
		return vars_push( INT_VAR, 1);

	// Open hid connection and ports

	if (! (hid = hid_open( MCC_VID, USB1024LS_PID, NULL ))) {
		print( FATAL, "Failed to open HID connection.\n" );
		THROW( EXCEPTION );
    }
	fsc2_usleep( 100000, UNSET );

	// Config Ports

	report.report_id = 0x0;  // always zero
	report.cmd = DCONFIG;
	report.port = DIO_PORTA;
	report.direction = DIO_DIR_OUT;

	if (hid_write( hid, (const unsigned char *) &report, sizeof report )) {
		print( FATAL, "Laied to wrte to device\n" );
		THROW( EXCEPTION );
	}
	fsc2_usleep( 100000, UNSET );

	//Send pulse
	//usbDOut_USB1024LS(hid, DIO_PORTA, 1);
  
	cmd [  0] = 0;     // report_id always zero
	cmd[ 0 ] = DOUT;
	cmd[ 1 ] = DIO_PORTA;
	cmd[ 2 ] = 1;

	if (hid_write( hid, cmd, sizeof cmd )) {
		print( FATAL, "Laied to wrte to device\n" );
		THROW( EXCEPTION );
	}
	fsc2_usleep( 200000, UNSET );

	// usbDOut_USB1024LS(hid, DIO_PORTA, 0);

	cmd[ 0 ] = 0;     // report_id always zero
	cmd[ 0 ] = DOUT;
	cmd[ 1 ] = DIO_PORTA;
	cmd[ 2 ] = 0;
	if (hid_write( hid, cmd, sizeof cmd )) {
		print( FATAL, "Laied to wrte to device\n" );
		THROW( EXCEPTION );
	}

	return vars_push(INT_VAR, 1);
}



