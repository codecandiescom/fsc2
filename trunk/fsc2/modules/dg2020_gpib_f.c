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
	int i;
	FUNCTION *f;


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

	if ( ! dg2020_set_timebase( dg2020.timebase ) )
		return FAIL;

	/* Set the memory size needed */

	if ( ! dg2020_set_memory_size( ( long ) dg2020.mem_size ) )
		return FAIL;

	/* If additional padding is needed create sequence and blocks */

	if ( dg2020.block[ 0 ].is_used && dg2020.block[ 1 ].is_used &&
		 ( ! dg2020_make_blocks( 2, dg2020.block ) ||
		   ! dg2020_make_seq( 2, dg2020.block ) ) )
		return FAIL;

	/* Do the assignement of channels to pods */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];
		if ( ! f->is_used )
			continue;

		dg2020_channel_assign( f->channel[ 0 ]->self, f->pod->self );

		if ( f->self == PULSER_CHANNEL_PHASE_1 &&
			 f->self == PULSER_CHANNEL_PHASE_2 )
		{
			dg2020_channel_assign( f->channel[ 1 ]->self, f->pod2->self );
			f->next_phase = 2;
		}
	}

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
/*  * 1: time base (in seconds) to be set                        */
/*  * 2: 1: ok, 0: error                                         */
/*---------------------------------------------------------------*/

bool dg2020_set_timebase( double timebase )
{
	char cmd[ 30 ] = "SOUR:OSC:INT:FREQ ";


	if ( timebase < 4.999e-9 || timebase > 10.001 )
		return FAIL;

	gcvt( 1.0 / timebase, 4, cmd + strlen( cmd ) );
	return gpib_write( dg2020.device, cmd, strlen( cmd ) ) == SUCCESS;
}


/*------------------------------------------------------------*/
/* dg2020_set_memory_size() sets the length (in clock cycles) */
/* of the complete pulse pattern.                             */
/* ->                                                         */
/*  * device number                                           */
/*  * length of the pulse pattern (between 64 and 64K)        */
/* <-                                                         */
/*  * 1: ok, 0: error                                         */
/*------------------------------------------------------------*/

bool dg2020_set_memory_size( long mem_size )
{
	char cmd[ 20 ] = "DATA:MSIZ ";


	if ( mem_size < 64 || mem_size > MAX_PULSER_BITS )
		return FAIL;

	sprintf( cmd + strlen( cmd ), "%ld\n", mem_size );
	return gpib_write( dg2020.device, cmd, strlen( cmd ) ) == SUCCESS;
}


/*-----------------------------------------------------*/
/* dg2020_channel_assign() assigns one of the channels */
/* of the pulser to one of the outputs of pod A.       */
/* ->                                                  */
/*  * channel number                                   */
/*  * pod output number                                */
/* <-                                                  */
/*  * 1: ok, 0: error                                  */
/*-----------------------------------------------------*/

bool dg2020_channel_assign( int channel, int pod )
{
	char cmd[ 30 ] = "OUTP:PODA:CH";


	if ( channel < 0 || channel >= MAX_CHANNELS ||
		 pod < 0 || pod >= MAX_PODS )
		return FAIL;

	sprintf( cmd + strlen( cmd ), "%d:ASSIGN ", pod );
	sprintf( cmd + strlen( cmd ), "%d", channel );

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


/*----------------------------------------------------------------------*/
/* dg2020_make_block() creates a complete new set of 'num_blks' blocks  */
/* (i.e. old blocks will be deleted) according to the names and start   */
/* positions given in the array of block descriptors 'block'.           */
/* No error checking is implemented yet !                               */
/* <-                                                                   */
/*  * 1: ok, 0: error                                                   */
/*----------------------------------------------------------------------*/

bool dg2020_make_blocks( int num_blks, BLOCK *block )
{
	char cmd[ 1024 ] = "", dummy[ 1000 ];
	long l;
	int i;

/*  According to the manual this should do the job - but it doesn't...

	for ( i = 0; i < num_blks; ++i )
	{
		sprintf( cmd + strlen( cmd ), "%ld,%s\n",
				 block[ i ].start, block[ i ].blk_name );
	}

	l = strlen( cmd ) - 1;
	sprintf( dummy, "%ld", l );
	sprintf( cmd, "DATA:BLOC:DEF #%ld%s", ( long ) strlen( dummy ), dummy );

	for ( i = 0; i < num_blks; ++i )
	{
		sprintf( cmd + strlen( cmd ), "%ld,%s\n",
				 block[ i ].start, block[ i ].blk_name );
	}
	cmd[ strlen( cmd ) - 1 ] = '\0';

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		return FAIL;

	gpib_write( dg2020.device, "*ESR?", 5 );
	l = 100;
	gpib_read( dg2020.device, dummy, &l );
	if ( dummy[ 0 ] != '0' || l != 2 )
	{
		gpib_write( dg2020.device, "ALLE?", 5 );
		l = 1000;
		gpib_read( dg2020.device, dummy, &l );
	}
*/

	/* ...so we try it this way - at least it does work.
	   Notice also the nice irregularity in the DEF and ADD command:
	   for DEF we need the block name without quotes while in ADD
	   we need quotes...*/

	sprintf( dummy, "%ld,%s", block[ 0 ].start, block[ 0 ].blk_name );
	l = strlen( dummy );
	sprintf( dummy, "%ld", l );
	l = strlen( dummy );
	sprintf( cmd, "DATA:BLOC:DEF #%ld%s", l, dummy );
	sprintf( cmd + strlen( cmd ), "%ld,%s",
			 block[ 0 ].start, block[ 0 ].blk_name );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		return FAIL;

	for ( i = 1; i < num_blks; ++i )
	{
		sprintf( cmd, "DATA:BLOC:ADD %ld,\"%s\"",
				 block[ i ].start, block[ i ].blk_name );
		if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
			return FAIL;
	}

	return OK;
}


/*--------------------------------------------------------------*/
/* dg2020_make_seq() creates a complete new sequence (i.e. old  */
/* sequences will be lost) consisting of 'num_blks' blocks with */
/* names and block repeat counts defined by the array of block  */
/* structures 'block'                                           */
/* ->                                                           */
/*  * number of blocks in sequence                              */
/*  * array of block structures                                 */
/* <-                                                           */
/*  * 1: ok, 0: error                                           */
/*--------------------------------------------------------------*/

bool dg2020_make_seq( int num_blks, BLOCK *block )
{
	char cmd[ 1024 ] = "", dummy[ 10 ];
	long l;
	int i;


	for ( i = 0; i < num_blks; ++i )
		sprintf( cmd + strlen( cmd ), "%s,%ld,0,0,0,0\n",
				 block[ i ].blk_name, block[ i ].repeat );

	l = strlen( cmd );
	sprintf( dummy, "%ld", l - 1 );
	l = strlen( dummy );
	sprintf( cmd, "DATA:SEQ:DEF #%ld%s", l, dummy );

	for ( i = 0; i < num_blks; ++i )
		sprintf( cmd + strlen( cmd ), "%s,%ld,0,0,0,0\n",
				 block[ i ].blk_name, block[ i ].repeat );
	cmd[ strlen( cmd ) - 1 ] = '\0';

	return gpib_write( dg2020.device, cmd, strlen( cmd ) ) == SUCCESS;
}


/*----------------------------------------------------*/
/* dg2020_set_channel() sets a bit pattern in one of  */
/* the channels of the pulser.                        */
/* ->                                                 */
/*  * channel number                                  */
/*  * start address for pattern (0 to 0xFFFF)         */
/*  * length of pattern (1 to 0xFFFF)                 */
/*    (address + length must be less or equal 0xFFFF) */
/*  * array with pattern (as a char array)            */
/* <-                                                 */
/*  * 1: ok, 0: error                                 */
/*----------------------------------------------------*/

bool pulser_set_channel( int channel, Ticks address,
						 Ticks length, char *pattern )
{
	char *cmd;
	Ticks k, l;
	int result;


	/* check parameters, allocate memory and set up start of command string */

	if ( ! dg2020_prep_cmd( &cmd, channel, address, length ) )
		return FAIL;

	/* assemble rest of command string */

	for ( k = 0, l = strlen( cmd ); k < length; ++k, ++l )
		cmd[ l ] = ( pattern[ k ] ? '1' : '0' );
	cmd[ l ] = '\0';

	/* send the command string to the pulser */

	result = gpib_write( dg2020.device, cmd, strlen( cmd ) );

	T_free( cmd );
	return result == SUCCESS;
}


/*-----------------------------------------------------*/
/* dg2020_set_constant() sets a certain number of bits */
/* in one of the channels either to high or low.       */
/* ->                                                  */
/*  * channel number                                   */
/*  * start address (0 to 0xFFFF)                      */
/*  * length of pattern (1 to 0xFFFF)                  */
/*    (address + length must be less or equal 0xFFFF)  */
/*  * either 1 or 0 to set the bits to high or low     */
/* <-                                                  */
/*  * 1: ok, 0: error                                  */
/*-----------------------------------------------------*/

bool dg2020_set_constant( int channel, Ticks address, Ticks length, int state )
{
	char *cmd, *cptr;
	Ticks k;
	Ticks m, n;
	int result;
	char s = ( state ? '1' : '0' );


	/* The following is a dirty hack to get around the 63K write limit
	   by splitting too long transfers into several shorter ones.
	   This should go away when the problem is finally solved !!!!!!! */

	if ( length > DMA_SIZE - 50 )
	{
		for ( m = address; m < length; m += DMA_SIZE - 50 )
		{
			if ( ( n = DMA_SIZE - 50 ) > length - m )
				n = length - m;
			if ( ! dg2020_set_constant( channel, m, n, state ) )
				return FAIL;
		}
		return OK;
	}

	/* check parameters, allocate memory and set up start of command string */

	if ( ! dg2020_prep_cmd( &cmd, channel, address, length ) )
		return FAIL;

	/* assemble rest of command string */

	for ( k = 0, cptr = cmd + strlen( cmd ); k < length; *cptr++ = s, ++k )
		;
	*cptr++ = '\n';
	*cptr = '\0';

	/* send the command string to the pulser */

	result = gpib_write( dg2020.device, cmd, strlen( cmd ) );

	T_free( cmd );       /* free memory used for command string */

	return result == SUCCESS;
}
