/*
  $Id$
*/



#include "dg2020.h"
#include "gpib.h"



/*------------------------------------------------------*/
/* dg2020_init() initializes the Sony/Tektronix DG2020. */
/* ->                                                   */
/*  * symbolic name of the device (cf gpib.conf)        */
/* <-                                                   */
/*  * 1: ok, 0: error                                   */
/*------------------------------------------------------*/

bool dg2020_init( const char *name )
{
	if ( gpib_init_device( name, &dg2020.device ) == FAILURE )
        return FAIL;

    /* Set pulser to short form of replies */

    if ( gpib_write( dg2020.device, "VERB OFF", 8 ) == FAILURE ||
         gpib_write( dg2020.device, "HEAD OFF", 8 ) == FAILURE )
        return FAIL;

	if ( gpib_write( dg2020.device, "STOP", 4 ) == FAILURE )
		return FAIL;
	dg2020.is_running = 0;

	/* switch off remote command debugging function */

	gpib_write( dg2020.device, "DEB:SNO:STAT OFF", 16 );

	/* switch on phase lock for internal oscillator */

	if ( gpib_write( dg2020.device, "SOUR:OSC:INT:PLL ON", 19 ) == FAILURE )
		return FAIL;

	/* delete all blocks */

	if ( gpib_write( dg2020.device, "DATA:BLOC:DEL:ALL", 17 ) == FAILURE )
		return FAIL;

	/* remove all sequence definitions */

	if ( gpib_write( dg2020.device, "DATA:SEQ:DEL:ALL", 16 ) == FAILURE )
		return FAIL;

	/* switch to manual update mode */

	if ( gpib_write( dg2020.device, "MODE:UPD MAN", 12 ) == FAILURE )
		return FAIL;

	/* switch to repetition mode */

	if ( gpib_write( dg2020.device, "MODE:STAT REP", 13 ) == FAILURE )
		return FAIL;

	/* set the time base */

	dg2020_set_timebase( );

    return OK;
}


/*--------------------------------------------------------------------*/
/* dg2020_run() sets the run mode of the the pulser - either running  */
/* or stopped - after waiting for previous commands to finish (that's */
/* what the "*WAI;" bit in the command is about)                      */
/* ->                                                                 */
/*  * state to be set: 1 = START, 0 = STOP                            */
/* <-                                                                 */
/*  * 1: ok, 0: error                                                 */
/*--------------------------------------------------------------------*/

bool dg2020_run( bool flag )
{
	if ( flag == dg2020.is_running )          // if in requested state
		return OK;

	if ( gpib_write( dg2020.device, flag ? "*WAI;STAR": "*WAI;STOP", 9 )
		 == FAILURE )
		return FAIL;

	dg2020.is_running = flag;
	return OK;
}


/*---------------------------------------------------------------*/
/* dg2020_set_timebase() sets the internal clock oscillator of   */
/* othe pulser to the period specified in the pulsers structure. */
/*  There might be minor deviations (in the order of a promille) */
/* between the specified period and the actual period.           */
/* <-                                                            */
/*  * 1: ok, 0: error                                            */
/*---------------------------------------------------------------*/

bool dg2020_set_timebase( void )
{
	char cmd[ 30 ] = "SOUR:OSC:INT:FREQ ";


	if ( dg2020.timebase < 4.999e-9 || dg2020.timebase > 10.001 )
		return FAIL;

	gcvt( 1.0 / dg2020.timebase, 4, cmd + strlen( cmd ) );
	return gpib_write( dg2020.device, cmd, strlen( cmd ) ) == SUCCESS;
}


/*--------------------------------------------------------------*/
/* dg2020_update_data() tells the pulser to update all channels */
/* according to the data it got send before - this is necessary */
/* because the pulser is used in manual update mode which this  */
/* is faster than automatic update.                             */
/* <-                                                           */
/*  * 1: ok, 0: error                                           */
/*--------------------------------------------------------------*/

bool dg2020_update_data( void )
{
	return gpib_write( dg2020.device, "DATA:UPD", 8 ) == SUCCESS;
}
