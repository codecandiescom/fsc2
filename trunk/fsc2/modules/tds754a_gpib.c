/*
  $Id$
*/



#include "tds754a.h"
#include "gpib.h"


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_init( const char *name )
{
	if ( gpib_init_device( name, &tds754a.device ) == FAILURE )
        return FAIL;

	/* Set digitzer into local lockout state */

	gpib_lock( tds754a.device );
    if ( gpib_write( tds754a.device, "LOC ALL", 7 ) == FAILURE )
	{
		gpib_local( tds754a.device );
        return FAIL;
	}

    /* Set digitizer to short form of replies */

    if ( gpib_write( tds754a.device, "VERB OFF", 8 ) == FAILURE ||
         gpib_write( tds754a.device, "HEAD OFF", 8 ) == FAILURE )
	{
		gpib_local( tds754a.device );
        return FAIL;
	}

    /* Get record length and trigger position */

    if ( ! tds754a_get_record_length( &tds754a.rec_len ) ||
         ! tds754a_get_trigger_pos( &tds754a.trig_pos ) )
	{
		gpib_local( tds754a.device );
        return FAIL;
	}

    /* Set format of data transfer (binary, INTEL format) */

    if ( gpib_write( tds754a.device, "DAT:ENC SRI", 11 ) == FAILURE ||
         gpib_write( tds754a.device, "DAT:WID 2", 9 )    == FAILURE )
	{
		gpib_local( tds754a.device );
        return FAIL;
	}
		
    /* Set unit for cursor setting commands to seconds, cursor types to VBAR */

    if ( gpib_write( tds754a.device, "CURS:FUNC VBA", 13 )       == FAILURE ||
         gpib_write( tds754a.device, "CURS:VBA:UNITS SECO", 19 ) == FAILURE )
    {
        gpib_local( tds754a.device );
        return FAIL;
    }

    /* Switch off repetitive acquisition mode */

	if ( gpib_write( tds754a.device, "ACQ:REPE OFF", 12 ) == FAILURE )
    {
        gpib_local( tds754a.device );
        return FAIL;
    }

	/* Check if the the time base has been set in the test run, if so send it
	   to the device, otherwise get it */

	if ( tds754a.is_timebase )
		tds754a_set_timebase( tds754a.timebase );
	else
		tds754a.timebase = tds754a_get_timebase( );

	/* If the number of averages has been set in the PREPARATIONS section send
       to the digitizer now */

	if ( tds754a.is_num_avg == SET )
		tds754a_set_num_avg( tds754a.num_avg );

    /* Switch to running until run/stop button is pressed and start running */

    if ( gpib_write( tds754a.device, "ACQ:STOPA RUNST", 15 ) == FAILURE ||
         gpib_write( tds754a.device, "ACQ:STATE RUN", 13 ) == FAILURE )
    {
        gpib_local( tds754a.device );
        return( FAIL );
    }

    /* Now we still have to prepare the digitzer for the measurement:
       1. set mode that digitizer stops after the number of averages set
       2. set OPC (operation complete) bit in Device Event Status Enable
          Register (DESER) -> corresponding bit in Standard Event Status
          Register (SESR) can be set
       3. set OPC bit also in Event Status Enable Register (ESER) ->
          ESB bit in status byte register can be set
       4. set ESB bit in Service Request Enable Register (SRER)-> SRQ can
          be flagged due to a set ESB-bit in Status Byte Register (SBR)
    */

    if ( gpib_write( tds754a.device, "ACQ:STOPA SEQ", 13 ) == FAILURE ||
         gpib_write( tds754a.device, "DESE 1", 6 )         == FAILURE ||
         gpib_write( tds754a.device, "*ESE 1", 6 )         == FAILURE ||
         gpib_write( tds754a.device, "*SRE 32", 7 )        == FAILURE )
    {
        gpib_local( tds754a.device );
        return FAIL;
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds754a_get_timebase( void )
{
	char reply[ 30 ];
	long length;


	if ( gpib_write( tds754a.device, "HOR:MAI:SCA?", 12 ) == FAILURE ||
		 gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

	reply[length - 1] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_set_timebase( double timebase )
{
	char command[40] = "HOR:MAI:SCA ";


	gcvt( timebase, 6, command + strlen( command ) );
	if ( gpib_write( tds754a.device, command, strlen( command ) ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/* tds754a_get_record_length() returns the current record length. */
/*----------------------------------------------------------------*/

bool tds754a_get_record_length( long *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds754a.device, "HOR:RECO?", 9 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = strtol( reply, NULL, 10 );
    return OK;
}

/*-----------------------------------------------------------------*/
/* tds754a_get_trigger_pos() returns the current trigger position. */
/*-----------------------------------------------------------------*/

bool tds754a_get_trigger_pos( double *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds754a.device, "HOR:TRIG:POS?", 13 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = 0.01 * T_atof( reply );
    return OK;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

long tds754a_get_num_avg( void )
{
	char reply[30];
	long length = 30;


	if ( tds754a_get_acq_mode() == AVERAGE )
	{
		if ( gpib_write( tds754a.device,"ACQ:NUMAV?", 10 ) == FAILURE ||
			 gpib_read( tds754a.device, reply, &length) == FAILURE )
			tds754a_gpib_failure( );

		reply[ length - 1 ] = '\0';
		return T_atol( reply );
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

bool tds754a_set_num_avg( long num_avg )
{
	char cmd[ 30 ];


	/* With number of acquisitions set to zero simply stop the digitizer */

	if ( num_avg == 0 )
	{
		if ( gpib_write( tds754a.device, "ACQ:STATE STOP", 14 ) == FAILURE )
			tds754a_gpib_failure( );
		return OK;
	}

	/* With number of acquisitions switch to sample mode, for all other numbers
	   set the number of acquisitions and switch to average mode */

	if ( num_avg == 1 )
	{
		if ( gpib_write( tds754a.device, "ACQ:MOD SAM", 11 ) == FAILURE )
			tds754a_gpib_failure( );
	}
	else
	{
		strcpy(cmd, "ACQ:NUMAV ");
		sprintf( cmd + strlen( cmd ), "%ld", num_avg );
		if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE ||
			 gpib_write( tds754a.device, "ACQ:MOD AVE", 11 ) == FAILURE )
			tds754a_gpib_failure( );
	}

	/* Finally restart the digitizer */

	if ( gpib_write( tds754a.device, "ACQ:STATE RUN", 13 ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}

/*-------------------------------------------------------------------------*/
/* Function returns the data acquisition mode. If the digitizer is neither */
/* in average nor in sample mode, it is switched to sample mode.           */ 
/*-------------------------------------------------------------------------*/

int tds754a_get_acq_mode(void)
{
	char reply[30];
	long length;


	if ( gpib_write( tds754a.device, "ACQ:MOD?", 8 ) == FAILURE ||
		 gpib_read ( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

	if ( *reply == 'A' )		/* digitizer is in average mode */
		return AVERAGE;

	if ( *reply != 'S' )		/* if not in sample mode set it */
	{
		if ( gpib_write( tds754a.device,"ACQ:MOD SAM", 11 ) == FAILURE )
			tds754a_gpib_failure( );
	}

	return SAMPLE;
}


/*----------------------------------------------------*/
/* tds754a_get_cursor_distance() returns the distance */
/* between the two cursors.                           */
/*----------------------------------------------------*/

bool tds754a_get_cursor_distance( double *cd )
{
    char reply[ 30 ];
    long length;


    if ( gpib_write( tds754a.device, "CURS:VBA:POSITION2?", 19 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cd = T_atof( reply );

    length = 30;
    if ( gpib_write( tds754a.device, "CURS:VBA:POSITION1?", 19 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cd -= T_atof( reply );

    return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_set_trigger_channel( const char *name )
{
	char cmd[ 40 ] = "TRIG:MAI:EDGE:SOU ";

	if ( strlen( name ) > 20 )
		return FAIL;

	strcat( cmd, name );

	if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

int tds754a_get_trigger_channel( void )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds754a.device, "TRIG:MAI:EDGE:SOU?", 18 ) == FAILURE ||
		 gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

    reply[3] = '\0';

	/* Possible replies are "CH1", "CH2", "CH3", "CH4", "AUX" or "LIN" */

    if ( ! strcmp( reply, "AUX" ) )
        return TDS754A_AUX;

    if ( ! strcmp( reply, "LIN" ) )
        return TDS754A_LIN;

    return ( int ) ( reply[ 2 ] - '1' );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

void tds754a_gpib_failure( void )
{
	eprint( FATAL, "TDS754A: Communication with device failed." );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------------*/
/* tds754a_clear_SESR() reads the the standard event status register */
/* and thereby clears it - if this isn't done no SRQs are flagged !  */
/*-------------------------------------------------------------------*/

bool tds754a_clear_SESR( void )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds754a.device, "*ESR?", 5 ) == FAILURE ||
		 gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*/
/* tds754a_finished() does all the work after an experiment is finished. */
/*-----------------------------------------------------------------------*/

void tds754a_finished( void )
{
    tds754a_clear_SESR( );
    gpib_write( tds754a.device, "ACQ:STATE STOP", 14 );

    gpib_write( tds754a.device, "*SRE 0", 5 );
    gpib_write( tds754a.device, "ACQ:STOPA RUNST", 15 );
    gpib_write( tds754a.device, "ACQ:STATE RUN", 13 );
    gpib_write( tds754a.device, "LOC NON", 7 ); 

	gpib_local( tds754a.device );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_set_cursor( int cur_num, double pos )
{
    char cmd[ 60 ] = "CURS:VBA:POSITION";


	assert( cur_num == 1 || cur_num == 2 );

    /* set cursors to specified positions */

	sprintf( cmd + strlen( cmd ), "%d", cur_num );
    gcvt( pos, 9, cmd + strlen( cmd ) );
    if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_set_track_cursors( bool flag )
{
	char cmd[ 20 ] = "CURS:MODE  ";

	strcat( cmd, flag ? "TRAC" : "IND" );
    if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*----------------------------------------------------*/
/* Function switches gated measurement mode on or off */
/*----------------------------------------------------*/

bool tds754a_set_gated_meas( bool flag )
{
	char cmd[ 20 ] = "MEASU:GAT ";

	strcat( cmd, flag ? "ON" : "OFF" );
    if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*-------------------------------------------------*/
/* Function switches on a channel of the digitizer */
/*-------------------------------------------------*/

bool tds754a_display_channel( int channel )
{
	char cmd[ 30 ] = "SEL:";
    char reply[ 10 ];
    long length = 10;


	assert( channel >= 0 && channel < TDS754A_AUX );

	/* check if channel is already displayed */

    strcat( cmd, Channel_Names[ channel ] );
    strcat( cmd, "?" );
    if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

    /* if not switch it on */

    if ( reply[ 0 ] == '0' )
    {
        strcpy( cmd, "SEL:" );
        strcat( cmd, Channel_Names[ channel ] );
        strcat( cmd, " ON" );
        if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds754a_gpib_failure( );
		sleep( 2 );                  // do we really need this ?
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_start_aquisition( void )
{
	int status;


    /* Start an acquisition:
       1. clear the SESR register to allow SRQs
       2. set state to run
       3. tell digitizer to set the operation complete bit in the SESR when
	      the measurement is finished, this, in turn will set the ESB-bit in
          the status byte register which leads to a SRQ */

    if ( ! tds754a_clear_SESR( ) ||
         gpib_write( tds754a.device, "ACQ:STATE RUN", 13 ) == FAILURE ||
         gpib_write( tds754a.device, "*OPC", 4 ) == FAILURE )
		tds754a_gpib_failure( );

    /* Now all left to do is to wait for a SRQ (don't test return status
       for RQS bit set - it obviously never is (?)) */

    if ( gpib_wait( tds754a.device, RQS, &status ) == FAILURE )
		tds754a_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

double tds754a_get_area( int channel, WINDOW *w )
{
	char cmd[ 50 ] = "MEASU:IMM:SOURCE ";
	char reply[ 40 ];
	long length = 40;


    /* Now we can measure the data:
       1. set measurement type to area
	   2. set the cursors if necessary
       2. measure all the areas:
          a. set channel (if the channel is not already set) 
          b. get the value of the area */

    if ( gpib_write( tds754a.device, "MEASU:IMM:TYP AREA", 18 ) == FAILURE )
		tds754a_gpib_failure( );

	assert( channel >= 0 && channel < TDS754A_AUX );

	if ( channel != tds754a.meas_source )
	{
		strcat( cmd, Channel_Names[ channel ] );
		if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds754a_gpib_failure( );
		tds754a.meas_source = channel;
	}

	tds754a_set_meas_window( w );

	if ( gpib_write( tds754a.device, "MEASU:IMM:VAL?", 14 ) == FAILURE ||
		 gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

	reply[ length - 1 ] = '\0';
	return T_atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

bool tds754a_get_curve( int channel, WINDOW *w, double **data, long *length )
{
	char cmd[ 40 ];


	assert( channel >= 0 && channel < TDS754A_AUX );

	if ( channel != tds754a.data_source )
	{
		strcpy( cmd, "DATA:SOURCE " );
		strcat( cmd, Channel_Names[ channel ] );
		if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
			tds754a_gpib_failure( );
		tds754a.data_source = channel;
	}

	tds754a_set_window( w );

	if ( gpib_write( tds754a.device, "CURV?", 5 ) == FAILURE )
		tds754a_gpib_failure( );

	/* !!!!!!!!!! Still some stuff to do */

	return OK;
}
