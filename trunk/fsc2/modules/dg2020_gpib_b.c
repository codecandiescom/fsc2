/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

   STILL COMPLETELY UNTESTED: Handling of EXTERNAL trigger mode by creating one
   block with "DATA:SEQ:TWAIT" for this block and running in enhanced mode. I
   urgently need access to a real device for testing if this is the way to do
   it - the manual is no help at all at trying to figure out what to do...

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/



/*
  <RANT>
  The guys that wrote the firmware of this digitizer as well as the
  documentation are not only a fucking bunch of morons but should be
  quartered, hanged and burned. Not only did they introduce some very annoying
  bugs but also made live a hell for the programmers that have no choice but
  to try to use their piece of crap.

  Here a few of the bugs as far as I found them yet (text taken from an email
  I sent to Tektronix/Sony):

  In run mode : Repeat
   When I create a data pattern starting with a high state (1) at the very
   first address in one of the channels and then start data pattern output
   by pressing the START/STOP button (or sending the START command via the
   GPIB interface) the DG2020 will output a pulse of 250 us length before
   starting to output the data pattern. It doesn't matter if there is a
   sequence defined or not, the 250 us pulse is always there and is easy
   visible with a digital oscilloscope when using resolutions and data
   pattern lengths in the nanosecond range.

  In run mode : Single
   In the same situation as above, i.e. a data pattern starting with high at
   the very first address, pressing the START/STOP button leads to an
   obviously infinite pulse (at least more than a minute) and no data
   pattern output is to be seen. If there is no data pattern bit set to high
   at the first address there is no output at all. This renders the Single
   mode complete unusable.

  I also had lots of problems using the GBIP commands as given in the
  manual. Some of the ones I tried to use don't work as described there.
  For example:

     a.) DATA:BLOCK:DEFINE <Blockinfo>
         this command works if one defines just one block but if one tries to
         define several blocks at once as described in the manual the command
         fails. E.g. the command
                  DATA:BLOC:DEF #2110,B1<LF>328,B2
         (where <LF> is the the line feed character 0x0A) results in a
               2075,"Illegal block size"
         error message (as returned by "ALLEV?") - and, of course, no blocks
         are defined.

         The only way around this error is to define the first block with
         DATA:BLOCK:DEFINE and use DATA:BLOCK:ADD to define the other blocks.
         (There is also a rather strange irregularity in these commands: while
         in the DEFINE command one has to enter the block name without quotes
         in the ADD command one needs quotes around the block name...)

     b.) DATA:PATTERN:BIT ...
         Normally this command results in a
               2022,"Pattern data byte count error"
         but works on some occasions - I was not able to figure out under what
         conditions (of course I checked and rechecked that the numbers
         given in the command were correct). Just by chance I found, that
         the command always seems to work if one preceeds it by a semicolon,
         i.e. as if it would be part of a concatenated command.

  Ok, so far about some real stupid fuck-ups in the firmware as well as in the
  documentation. But these guys also obviously spend so much time writing a
  more or less completely useless user interface that they didn't had the time
  to implement reasonable GPIB commands. E.g., who can be that stupid to write
  a command for setting pattern bits in a way that you have to send a complete
  byte over the already slow enough GPIB bus just to set one single bit of a
  pulser channel? And why isn't there at least a command to set a block of
  pattern bits by sending the start end length plus the state? And, finally,
  why isn't there for more than 90% of the stuff that you can do via the
  keyboard an equivalent GPIB command?

  Ok, they implemented the possibility to simulate all the things that you can
  do via the keyboard with the "ABSTouch" command but than made it complete
  useless by not giving the programmer a chance to find out in which mode the
  display is and where the cursors currently are etc...  That makes writing
  stuff using this feature like manually setting the digitizer without being
  able to watch the display. And even if this would be possible I really don't
  want to sent 25 commands down the GPIB bus just to simulate a complete
  rotation of a knobs (when you may need 20 rotations or more), or 35 commands
  just to move the cursor from channel 35 up to channel 0. Fucking idiots!

  Why do they have to have a minimum block size of 64 bits? Without this
  limitation you could do a lot of things in a real nice way, but so all you
  can do is to write a lot of bloody hacks and really cool things are simply
  impossible to do.

  Just another problem with the documentation: Why is there no comprehensible
  explanation of how to run the pulser in repeat mode but each repetition of
  the sequence starting on a trigger in event? Am I supposed to be clairvoyant
  or do these guys expect that I spend hours writing test programs until I
  find out what mode to use which what kinds of settings?

  And when you sent them emails (via their local distributor) to ask about
  these problems they not only let you wait for eternities (in my case half a
  year) but don't answer the most important question at all (the problem with
  the 250 us/infinitely long pulses) and with lots of bullshit about the
  others. And all this in a nearly incomprehensible english. Before this I had
  the impression that Tektronix's devices are quite good and especially the
  programming interface was real good but since they have been bought up by
  Sony much seems to have gone down the drain. Obviously, people at Sony have
  a problem understanding the difference between a walkman and a pulse data
  generator...

  </RANT>

*/


#include "dg2020_b.h"


/* Functions that re used only locally... */

static bool dg2020_set_timebase( double timebase );
static bool dg2020_set_memory_size( long mem_size );
static bool dg2020_set_pod_high_level( int pod, double voltage );
static bool dg2020_set_pod_low_level( int pod, double voltage );
static bool dg2020_set_trigger_in_level( double voltage );
static bool dg2020_set_trigger_in_slope( int slope );
static bool dg2020_set_trigger_in_impedance( int state );
static void dg2020_gpib_failure( void );


#ifdef DG2020_B_GPIB_DEBUG
#warning "*************************************"
#warning "dg2020_b.so made for DEBUG mode only!"
#warning "*************************************"

#define gpib_write( a, b, c ) ( fprintf( stderr, "%s\n", ( b ) ), SUCCESS )
#define gpib_read( a, b, c ) ( SUCCESS )
#define gpib_init_device( a, b ) 1
#endif



/*------------------------------------------------------*/
/* dg2020_init() initializes the Sony/Tektronix DG2020. */
/* ->                                                   */
/*  * symbolic name of the device (cf gpib.conf)        */
/* <-                                                   */
/*  * 1: ok, 0: error                                   */
/*------------------------------------------------------*/

bool dg2020_init( const char *name )
{
	int i, j;
	FUNCTION *f;
#ifndef DG2020_B_GPIB_DEBUG
	char reply[ 100 ];
	long len = 100;
#else
	name = name;
#endif


	if ( gpib_init_device( name, &dg2020.device ) == FAILURE )
		return FAIL;

	/* Try to get the status byte to make sure the pulser reacts */

	if ( gpib_write( dg2020.device, "*STB?\n", 6 ) == FAILURE ||
		 gpib_read( dg2020.device, reply, &len ) == FAILURE )
		dg2020_gpib_failure( );

    /* Set pulser to short form of replies */

    if ( gpib_write( dg2020.device, "VERB OFF\n", 9 ) == FAILURE ||
         gpib_write( dg2020.device, "HEAD OFF\n", 9 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Make sure the pulser is stopped */

	if ( gpib_write( dg2020.device, "STOP\n", 5 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Switch off remote command debugging function */

	if ( gpib_write( dg2020.device, "DEB:SNO:STAT OFF\n", 17 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Switch on phase lock for internal oscillator */

	if ( gpib_write( dg2020.device, "SOUR:OSC:INT:PLL ON\n", 20 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Delete all blocks */

	if ( gpib_write( dg2020.device, "DATA:BLOC:DEL:ALL\n", 18 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Remove all sequence definitions */

	if ( gpib_write( dg2020.device, "DATA:SEQ:DEL:ALL\n", 17 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Switch to manual update mode */

	if ( gpib_write( dg2020.device, "MODE:UPD MAN\n", 13 ) == FAILURE )
		dg2020_gpib_failure( );

	/* Set the time base */

	if ( ! dg2020_set_timebase( dg2020.timebase ) )
		dg2020_gpib_failure( );

	/* Set the memory size needed */

	if ( ! dg2020_set_memory_size( ( long ) dg2020.mem_size ) )
		dg2020_gpib_failure( );

	/* Switch on repeat mode for INTERNAL trigger mode and enhanced mode for
	   EXTERNAL trigger mode - in the later case also set trigger level and
	   slope. */

	if ( dg2020.trig_in_mode == INTERNAL )
	{
		if ( gpib_write( dg2020.device, "MODE:STAT REP\n", 14 )
			 == FAILURE )
			dg2020_gpib_failure( );
	}
	else
	{
		if ( gpib_write( dg2020.device, "MODE:STAT ENH\n", 14 )
			 == FAILURE )
			dg2020_gpib_failure( );
		if ( dg2020.is_trig_in_level )
			dg2020_set_trigger_in_level( dg2020.trig_in_level );
		if ( dg2020.is_trig_in_slope )
			dg2020_set_trigger_in_slope( dg2020.trig_in_slope );
		if ( dg2020.is_trig_in_impedance )
			dg2020_set_trigger_in_impedance( dg2020.trig_in_impedance );
	}

	/* If additional padding is needed or trigger mode is EXTERNAL create
	   sequence and blocks */

	if ( dg2020.block[ 0 ].is_used && dg2020.block[ 1 ].is_used &&
		 ( ! dg2020_make_blocks( 2, dg2020.block ) ||
		   ! dg2020_make_seq( 2, dg2020.block ) ) )
		dg2020_gpib_failure( );

	if ( dg2020.block[ 0 ].is_used && ! dg2020.block[ 1 ].is_used &&
		 ( ! dg2020_make_blocks( 1, dg2020.block ) ||
		   ! dg2020_make_seq( 1, dg2020.block ) ) )
		dg2020_gpib_failure( );

	/* Do the assignment of channels to pods */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = dg2020.function + i;
		if ( ! f->is_used )
			continue;

		if ( f->num_pods == 1 )
			dg2020_channel_assign( f->channel[ 0 ]->self,
								   f->pod[ 0 ]->self );
		else
			for ( j = 0; j < NUM_PHASE_TYPES; j++ )
				if ( f->phase_setup->is_set[ j ] )
					dg2020_channel_assign( f->pcm[ j * f->pc_len + 0 ]->self,
										   f->phase_setup->pod[ j ]->self );
	}

	/* Set up the pod output voltages */

	for ( i = 0; i < MAX_PODS; i++ )
	{
		if ( dg2020.pod[ i ].function == NULL )
			continue;

		if ( dg2020.pod[ i ].function->is_high_level )
			dg2020_set_pod_high_level( i,
									   dg2020.pod[ i ].function->high_level );
		if ( dg2020.pod[ i ].function->is_low_level )
			dg2020_set_pod_low_level( i,
									  dg2020.pod[ i ].function->low_level );
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
	if ( gpib_write( dg2020.device, flag ? "*WAI;STAR\n": "*WAI;STOP\n", 10 )
		 == FAILURE )
		dg2020_gpib_failure( );

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

static bool dg2020_set_timebase( double timebase )
{
	char cmd[ 30 ] = "SOUR:OSC:INT:FREQ ";


	if ( timebase < MIN_TIMEBASE * 0.99999 ||
		 timebase > MAX_TIMEBASE * 1.00001 )
		return FAIL;

	gcvt( 1.0 / timebase, 4, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
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

static bool dg2020_set_memory_size( long mem_size )
{
	char cmd[ 50 ];


	fsc2_assert( mem_size >= 64 && mem_size <= MAX_PULSER_BITS );

	sprintf( cmd, ":DATA:MSIZ %ld\n", mem_size );
	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
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
	char cmd[ 50 ];


	fsc2_assert( channel >= 0 && channel < MAX_CHANNELS &&
				 pod >= 0 && pod < MAX_PODS );

	sprintf( cmd, "OUTP:PODA:CH%d:ASSIGN %d\n", pod, channel );
	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
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
	if ( gpib_write( dg2020.device, "DATA:UPD\n", 9 ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------------------------*/
/* dg2020_make_block() creates a complete new set of 'num_blocks' blocks */
/* (i.e. old blocks will be deleted) according to the names and start    */
/* positions given in the array of block descriptors 'block'.            */
/* No error checking is implemented yet !                                */
/* <-                                                                    */
/*  * 1: ok, 0: error                                                    */
/*-----------------------------------------------------------------------*/

bool dg2020_make_blocks( int num_blocks, BLOCK *block )
{
	char cmd[ 1024 ] = "",
		 dummy[ 1000 ];
	long l;
	int i;


	/* Notice the nice irregularity in the DEF and ADD command: for DEF we
	   need the block name without quotes while in ADD we need quotes...*/

	sprintf( dummy, "%ld,%s\n", block[ 0 ].start, block[ 0 ].blk_name );
	l = strlen( dummy ) - 1;
	sprintf( dummy, "%ld", l );
	l = strlen( dummy );
	sprintf( cmd, ":DATA:BLOC:DEF #%ld%s%ld,%s\n", l, dummy,
			 block[ 0 ].start, block[ 0 ].blk_name );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	for ( i = 1; i < num_blocks; ++i )
	{
		sprintf( cmd, ":DATA:BLOC:ADD %ld,\"%s\"\n",
				 block[ i ].start, block[ i ].blk_name );
		if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
			dg2020_gpib_failure( );
	}

	return OK;
}


/*----------------------------------------------------------------*/
/* dg2020_make_seq() creates a completely new sequence (i.e. old  */
/* sequences will be lost) consisting of 'num_blocks' blocks with */
/* names and block repeat counts defined by the array of BLOCK    */
/* structures 'block'                                             */
/* ->                                                             */
/*  * number of blocks in sequence                                */
/*  * array of block structures                                   */
/* <-                                                             */
/*  * 1: ok, 0: error                                             */
/*----------------------------------------------------------------*/

bool dg2020_make_seq( int num_blocks, BLOCK *block )
{
	char cmd[ 1024 ] = "",
		 dummy[ 1024 ];
	long l;
	int i;


	l = 10 + strlen( block[ 0 ].blk_name );
	sprintf( dummy, "%ld", l );
	l = strlen( dummy );
	sprintf( cmd, ":DATA:SEQ:DEF #%ld%s%s,1,0,0,0,0\n",
			 l, dummy, block[ 0 ].blk_name );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	for ( i = 1; i < num_blocks; i++ )
	{
		sprintf( cmd, ":DATA:SEQ:ADD %d,\"%s\",%ld,0,0,0,0\n",
				 i, block[ i ].blk_name, block[ i ].repeat );
		if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
			dg2020_gpib_failure( );
	}

	/* For external trigger mode set trigger wait for first (and only) block */

	if ( dg2020.trig_in_mode == EXTERNAL &&
		 ( gpib_write( dg2020.device, ":DATA:SEQ:REP 0,1\n", 18 ) == FAILURE ||
		   gpib_write( dg2020.device, ":DATA:SEQ:TWAIT 0,1\n", 20 )
		   == FAILURE ) )
		dg2020_gpib_failure( );

	return OK;
}


/*-----------------------------------------------------*/
/* dg2020_set_constant() sets a certain number of bits */
/* in one of the channels either to high or low.       */
/* ->                                                  */
/*  * channel number                                   */
/*  * start address (-1 to 0xFFFE)                     */
/*  * length of pattern (1 to 0xFFFF)                  */
/*    (address + length must be less or equal 0xFFFF)  */
/*  * either 1 or 0 to set the bits to high or low     */
/* <-                                                  */
/*  * 1: ok, 0: error                                  */
/*-----------------------------------------------------*/

bool dg2020_set_constant( int channel, Ticks address, Ticks length, int state )
{
	char *cmd, *cptr;
#if defined DMA_SIZE
	Ticks m, n;
#endif


	address++;        /* because of the first unset bit */

#if defined DMA_SIZE

	/* The following is a dirty hack to get around the 63K write limit by
	   splitting transfers that would be too long into several shorter ones.
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
#endif

	/* Check parameters, allocate memory and set up start of command string */

	if ( ! dg2020_prep_cmd( &cmd, channel, address, length ) )
		return FAIL;

	/* Assemble rest of command string */

	cptr = cmd + strlen( cmd );
	memset( ( void * ) cptr, state ? '1' : '0', length );
	strcpy( cptr + length, "\n" );

	/* Send the command string to the pulser */

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
	{
		T_free( cmd );
		dg2020_gpib_failure( );
	}

	T_free( cmd );       /* free memory used for command string */

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool dg2020_set_pod_high_level( int pod, double voltage )
{
	char cmd[ 100 ];

	sprintf( cmd, "OUTP:PODA:CH%d:HIGH %f %s\n", pod,
			 fabs( voltage ) >= 1 ? voltage : 1000.0 * voltage,
			 fabs( voltage ) >= 1 ? "V" : "mV" );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool dg2020_set_pod_low_level( int pod, double voltage )
{
	char cmd[ 100 ];

	sprintf( cmd, "OUTP:PODA:CH%d:LOW %f %s\n", pod,
			 fabs( voltage ) >= 1 ? voltage : 1000.0 * voltage,
			 fabs( voltage ) >= 1 ? "V" : "mV" );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool dg2020_set_trigger_in_level( double voltage )
{
	char cmd[ 100 ];

	sprintf( cmd, "TRIG:LEV %f %s\n",
			 fabs( voltage ) >= 1 ? voltage : 1000.0 * voltage,
			 fabs( voltage ) >= 1 ? "V" : "mV" );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool dg2020_set_trigger_in_slope( int slope )
{
	char cmd[ 100 ];

	sprintf( cmd, "TRIG:SLO %s\n",
			 slope == POSITIVE ? "POS" : "NEG" );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static bool dg2020_set_trigger_in_impedance( int state )
{
	char cmd[ 100 ];

	sprintf( cmd, "TRIG:IMP %s\n",
			 state == LOW ? "LOW" : "HIGH" );

	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

static void dg2020_gpib_failure( void )
{
	print( FATAL, "Communication with device failed.\n" );
	THROW( EXCEPTION );
}


/*--------------------------------------------------------------*/
/*--------------------------------------------------------------*/

bool dg2020_lock_state( bool lock )
{
	char cmd[ 10 ];

	sprintf( cmd, "LOC %s\n", lock ? "ALL" : "NON" );
	if ( gpib_write( dg2020.device, cmd, strlen( cmd ) ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
