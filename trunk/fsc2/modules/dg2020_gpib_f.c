/*
  $Id$
*/

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

   STILL COMPLETELY UNTESTED: Handling of EXTERNAL trigger mode by creating one
   block with "DATA:SEQ:TWAIT" for this block and running in enhanced mode. I
   urgently need access to a real device for testing if this is the way to do
   it - the manual is no help at all at trying to figure out what to do...

   Is this still valid?  2000/7/9

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/



/*
  <RANT>

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
  pulser channel? And why isn't there for more than 90% of the stuff that you
  can do via the keyboard an equivalent GPIB command?

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
  can do is to write a lot of bloody hacks but really useful things are simply
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



#include "dg2020_f.h"
#include "gpib_if.h"

typedef struct _dg2020_store_ {
	int channel;
	Ticks address;
	Ticks length;
	int state;
	struct _dg2020_store_ *next;
	struct _dg2020_store_ *prev;
} DG2020_STORE;

static void dg2020_gpib_store( int channel, Ticks address, Ticks length,
							   int state );
static bool dg2020_gpib_check( void );
static bool dg2020_gpib_do_check( DG2020_STORE *params );


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

    if ( gpib_write( dg2020.device, "VERB OFF\n" ) == FAILURE ||
         gpib_write( dg2020.device, "HEAD OFF\n" ) == FAILURE )
		dg2020_gpib_failure( );

	/* Make sure the pulser is stopped */

	if ( gpib_write( dg2020.device, "STOP\n" ) == FAILURE )
		dg2020_gpib_failure( );
	dg2020.is_running = 0;

	/* Switch off remote command debugging function */

	if ( gpib_write( dg2020.device, "DEB:SNO:STAT OFF\n" ) == FAILURE )
		dg2020_gpib_failure( );

	/* Switch on phase lock for internal oscillator */

	if ( gpib_write( dg2020.device, "SOUR:OSC:INT:PLL ON\n" ) == FAILURE )
		dg2020_gpib_failure( );

	/* Delete all blocks */

	if ( gpib_write( dg2020.device, "DATA:BLOC:DEL:ALL\n" ) == FAILURE )
		dg2020_gpib_failure( );

	/* Remove all sequence definitions */

	if ( gpib_write( dg2020.device, "DATA:SEQ:DEL:ALL\n" ) == FAILURE )
		dg2020_gpib_failure( );

	/* Switch to manual update mode */

	if ( gpib_write( dg2020.device, "MODE:UPD MAN\n" ) == FAILURE )
		dg2020_gpib_failure( );

	/* Set the time base */

	if ( ! dg2020_set_timebase( dg2020.timebase ) )
		dg2020_gpib_failure( );

	/* Set the memory size needed */

	if ( ! dg2020_set_memory_size( ( long ) dg2020.mem_size ) )
		dg2020_gpib_failure( );

	/* Switch on repeat mode for INTERNAL trigger mode and enhanced mode for
	   EXTERNAL trigger mode - in the later case also set trigger level and
	   slope */

	if ( dg2020.trig_in_mode == INTERNAL )
	{
		if ( gpib_write( dg2020.device, "MODE:STAT REP\n" ) == FAILURE )
			dg2020_gpib_failure( );
	}
	else
	{
		if ( gpib_write( dg2020.device, "MODE:STAT ENH\n" ) == FAILURE )
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

	/* Do the assignement of channels to pods */

	for ( i = 0; i < PULSER_CHANNEL_NUM_FUNC; i++ )
	{
		f = &dg2020.function[ i ];
		if ( ! f->is_used )
			continue;

		dg2020_channel_assign( f->channel[ 0 ]->self, f->pod->self );

		if ( f->self == PULSER_CHANNEL_PHASE_1 ||
			 f->self == PULSER_CHANNEL_PHASE_2 )
		{
			dg2020_channel_assign( f->channel[ 1 ]->self, f->pod2->self );
			f->next_phase = 2;
		}
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
	if ( flag == dg2020.is_running )          // if in requested state
		return OK;

	if ( gpib_write( dg2020.device, flag ? "*WAI;STAR\n": "*WAI;STOP\n" )
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

bool dg2020_set_timebase( double timebase )
{
	char cmd[ 30 ] = "SOUR:OSC:INT:FREQ ";


	if ( timebase < MIN_TIMEBASE * 0.99999 ||
		 timebase > MAX_TIMEBASE * 1.00001 )
		return FAIL;

	gcvt( 1.0 / timebase, 4, cmd + strlen( cmd ) );
	strcat( cmd, "\n" );
	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
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

bool dg2020_set_memory_size( long mem_size )
{
	char cmd[ 20 ] = "DATA:MSIZ ";


	if ( mem_size < 64 || mem_size > MAX_PULSER_BITS )
		return FAIL;

	sprintf( cmd + strlen( cmd ), "%ld\n", mem_size );
	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
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
	char cmd[ 30 ] = "OUTP:PODA:CH";


	if ( channel < 0 || channel >= MAX_CHANNELS ||
		 pod < 0 || pod >= MAX_PODS )
		return FAIL;

	sprintf( cmd + strlen( cmd ), "%d:ASSIGN ", pod );
	sprintf( cmd + strlen( cmd ), "%d\n", channel );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
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
	if ( gpib_write( dg2020.device, "DATA:UPD\n" ) == FAILURE )
		dg2020_gpib_failure( );

	return dg2020_gpib_check( );
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

/*  According to the manual this should do the job - but it doesn't...

	for ( i = 0; i < num_blocks; ++i )
	{
		sprintf( cmd + strlen( cmd ), "%ld,%s\n",
				 block[ i ].start, block[ i ].blk_name );
	}

	l = strlen( cmd ) - 1;
	sprintf( dummy, "%ld", l );
	sprintf( cmd, "DATA:BLOC:DEF #%ld%s", ( long ) strlen( dummy ), dummy );

	for ( i = 0; i < num_blocks; ++i )
	{
		sprintf( cmd + strlen( cmd ), "%ld,%s\n",
				 block[ i ].start, block[ i ].blk_name );
	}
	cmd[ strlen( cmd ) - 1 ] = '\0';

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	if ( gpib_write( dg2020.device, "*ESR?\n" ) == FAILUE )
		dg2020_gpib_failure( );
	l = 100;
	if ( gpib_read( dg2020.device, dummy, &l ) == FAILURE )
		dg2020_gpib_failure( );
	if ( dummy[ 0 ] != '0' || l != 2 )
	{
		gpib_write( dg2020.device, "ALLE?\n" );
		l = 1000;
		if ( gpib_read( dg2020.device, dummy, &l ) == FAILURE )
			dg2020_gpib_failure( );
	}
*/

	/* ...so we try it this way - at least it does work.
	   Notice also the nice irregularity in the DEF and ADD command:
	   for DEF we need the block name without quotes while in ADD
	   we need quotes...*/

	sprintf( dummy, "%ld,%s\n", block[ 0 ].start, block[ 0 ].blk_name );
	l = strlen( dummy );
	sprintf( dummy, "%ld", l );
	l = strlen( dummy );
	sprintf( cmd, "DATA:BLOC:DEF #%ld%s", l, dummy );
	sprintf( cmd + strlen( cmd ), "%ld,%s\n",
			 block[ 0 ].start, block[ 0 ].blk_name );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	for ( i = 1; i < num_blocks; ++i )
	{
		sprintf( cmd, "DATA:BLOC:ADD %ld,\"%s\"\n",
				 block[ i ].start, block[ i ].blk_name );
		if ( gpib_write( dg2020.device, cmd ) == FAILURE )
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
		 dummy[ 10 ];
	long l;
	int i;


	for ( i = 0; i < num_blocks; ++i )
		sprintf( cmd + strlen( cmd ), "%s,%ld,0,0,0,0\n",
				 block[ i ].blk_name, block[ i ].repeat );

	l = strlen( cmd );
	sprintf( dummy, "%ld", l - 1 );
	l = strlen( dummy );
	sprintf( cmd, "DATA:SEQ:DEF #%ld%s", l, dummy );

	for ( i = 0; i < num_blocks; ++i )
		sprintf( cmd + strlen( cmd ), "%s,%ld,0,0,0,0\n",
				 block[ i ].blk_name, block[ i ].repeat );
	cmd[ strlen( cmd ) - 1 ] = '\0';

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	/* For external trigger mode set trigger wait for first (and only) block */

	if ( dg2020.trig_in_mode == EXTERNAL &&
		 ( gpib_write( dg2020.device, "DATA:SEQ:REP 0,1\n" ) == FAILURE ||
		   gpib_write( dg2020.device, "DATA:SEQ:TWAIT 0,1\n" ) == FAILURE )
		)
		dg2020_gpib_failure( );

	return OK;
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
	char s = ( state ? '1' : '0' );

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

	/* In order to check that the command really arrived at the pulser and has
	   been executed we have to store each change and compare it with the
	   actual state after an update comand */

	dg2020_gpib_store( channel, address, length, state );

	/* Check parameters, allocate memory and set up start of command string.
	   Because the very first bit in a channel alway has to stay unset we
	   have not to use address but the increment of address! */ 

	if ( ! dg2020_prep_cmd( &cmd, channel, address + 1, length ) )
		return FAIL;

	/* Assemble rest of command string */

	for ( k = 0, cptr = cmd + strlen( cmd ); k < length; *cptr++ = s, ++k )
		;
	strcpy( cptr, "\n" );

	/* Send the command string to the pulser */

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	T_free( cmd );       /* free memory used for command string */

	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool dg2020_set_pod_high_level( int pod, double voltage )
{
	char cmd[ 100 ];

	sprintf( cmd, "OUTP:PODA:CH%d:HIGH %f %s\n", pod,
			 fabs( voltage ) >= 1 ? voltage : 1000.0 * voltage,
			 fabs( voltage ) >= 1 ? "V" : "mV" );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool dg2020_set_pod_low_level( int pod, double voltage )
{
	char cmd[ 100 ];

	sprintf( cmd, "OUTP:PODA:CH%d:LOW %f %s\n", pod,
			 fabs( voltage ) >= 1 ? voltage : 1000.0 * voltage,
			 fabs( voltage ) >= 1 ? "V" : "mV" );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool dg2020_set_trigger_in_level( double voltage )
{
	char cmd[ 100 ];

	sprintf( cmd, "TRIG:LEV %f %s\n",
			 fabs( voltage ) >= 1 ? voltage : 1000.0 * voltage,
			 fabs( voltage ) >= 1 ? "V" : "mV" );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool dg2020_set_trigger_in_slope( int slope )
{
	char cmd[ 100 ];

	sprintf( cmd, "TRIG:SLO %s\n",
			 slope == POSITIVE ? "POS" : "NEG" );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

bool dg2020_set_trigger_in_impedance( int state )
{
	char cmd[ 100 ];

	sprintf( cmd, "TRIG:IMP %s\n",
			 state == LOW ? "LOW" : "HIGH" );

	if ( gpib_write( dg2020.device, cmd ) == FAILURE )
		dg2020_gpib_failure( );

	return OK;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

void dg2020_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n",
			pulser_struct.name );
	THROW( EXCEPTION );
}


/*-------------------------------------------------------------------------*/
/* Unfortunately, the Frankfurt pulser sometimes doesn't execute a command */
/* to set or clear some bits in a channel. Since I don't have any idea why */
/* but this can completely destroy a measurement the following is a hack   */
/* to avoid this kind of errors:                                           */
/* Each time bits are set this is stored by a call of dg2020_gpib_store(). */
/* Now, after each update of the pulser by a call of dg2020_update_data()  */
/* dg2020_gpib_check() is called. Here the patterns changed by previous    */
/* commands are read in from the pulser and compared with the state they   */
/* should be in (this can't be done immediately after the setting because  */
/* we get a valid reply only after the update command). If we detect a     */
/* difference the corresponding command is repeated and the result again   */
/* is checked. In order to avoid getting into an infinite loop of failed   */
/* commands this cycle is only repeated 'MAX_CHECK_RETRIES' times (see     */
/* definition below).                                                      */
/* Unfortunately, this checking will increase the time to setup the pulser */
/* quite a lot. To get rid of the checking just comment out the line       */
/*                                                                         */
/* 	   dg2020_gpib_store( channel, address, length, state );               */
/*                                                                         */
/* in the function dg2020_set_constant().                                  */
/*-------------------------------------------------------------------------*/

#define MAX_CHECK_RETRIES 3

static DG2020_STORE *dst = NULL;
static int check_retries = 0;


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static void dg2020_gpib_store( int channel, Ticks address, Ticks length,
							   int state )
{
	DG2020_STORE *cur = dst;


	/* Setup a linked list of all pulse changing commands */

	if ( dst == NULL )
	{
		dst = cur = T_malloc( sizeof( DG2020_STORE ) );
		cur->prev = cur->next = NULL;
	}
	else
	{
		while ( cur->next != NULL )
			cur = cur->next;

		cur->next = T_malloc( sizeof( DG2020_STORE ) );
		cur->next->prev = cur;
		cur = cur->next;
		cur->next = NULL;
	}

	cur->channel = channel;
	cur->address = address;
	cur->length = length;
	cur->state = state;
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static bool dg2020_gpib_check( void )
{
	DG2020_STORE *cur = dst,
		         *tmp,
		         *old_dst;
	bool result = OK;


	/* Return immediately if there's nothing to check */

	if ( dst == NULL )
		return OK;

	/* Do a check on each of the commands - if everything is ok remove the
	   entry from the list. So we end up with a list of just the commands
	   that failed.*/

	while ( cur != NULL )
	{
		if ( dg2020_gpib_do_check( cur ) )           /* everything ok ? */
		{
			if ( cur->prev != NULL )
				cur->prev->next = cur->next;
			if ( cur->next != NULL )
				cur->next->prev = cur->prev;
			if ( cur == dst )
				dst = cur->next;
			tmp = cur->next;
			T_free( cur );
			cur = tmp;
		}
		else
		{
			gpib_log_message( "dg2020_set_constant( %d, %ld, %ld, %d ) "
							  "failed.\n", cur->channel, cur->address,
							  cur->length, cur->state );
			result = FAIL;
			cur = cur->next;
		}
	}

	/* If everything is ok the command list is removed and we just return */

	if ( result )
	{
		check_retries = 0;
		return OK;
	}

	/* We give up if there were already enough retries - this is necessary to
       avoid getting into an infinite loop of failed commands */

	if ( check_retries++ >= MAX_CHECK_RETRIES )
	{
		/* Throw away the list of failed commands */

		while ( dst != NULL )
		{
			tmp = dst->next;
			T_free( dst );
			dst = tmp;
		}

		check_retries = 0;
		return FAIL;
	}

	/* Redo all the failed commands - we still need the list of the old
       commands to deallocate it while the retries create a new list */

	cur = old_dst = dst;
	dst = NULL;
	result = OK;

	while ( cur != NULL )
	{
		if ( ! dg2020_set_constant( cur->channel, cur->address, cur->length,
									cur->state ) )
		{
			result = FAIL;
			break;
		}

		cur = cur->next;
	}

	/* Get rid of the old list */

	while ( old_dst != NULL )
	{
		tmp = old_dst->next;
		T_free( old_dst );
		old_dst = tmp;
	}

	/* If redoing the commands failed we're out of options - we just delete
	   the newly created command list and return the failure status */

	if ( result != OK )
	{
		while ( dst != NULL )
		{
			tmp = dst->next;
			T_free( dst );
			dst = tmp;
		}

		check_retries = 0;
		return FAIL;
	}

	/* Now, to commit the changes, we call the update function - this, in
	   turn, leads to the checks done here being called again */

	return dg2020_update_data( );
}


/*----------------------------------------------------------------*/
/*----------------------------------------------------------------*/

static bool dg2020_gpib_do_check( DG2020_STORE *params )
{
	char cmd[ 100 ] = "DATA:PATT:BIT? ";
	char *reply;
	char *ptr;
	long length;
	long got_count;
	int got_count_digits;
	char state_char = ( params->state ? '1' : '0' );
	int i;
	

	/* Let's be paranoid */

	if ( params->length < 1 )
		return OK;

	/* Allocate enough space for the pulsers reply */

	length = params->length + 30;
	reply = T_malloc( length * sizeof( char ) );

	/* Assemble the command string - because of the first bit problem we have
	   to use the increment of address, see also dg2020_set_constant() */

	sprintf( cmd + strlen( cmd ), "%d,%ld,%ld\n", params->channel,
			 params->address + 1, params->length );

	/* Send command and receive reply */

	if ( gpib_write( dg2020.device, cmd ) == FAILURE ||
		 gpib_read( dg2020.device, reply, &length ) == FAILURE )
	{
		T_free( reply );
		dg2020_gpib_failure( );
	}

	/* The interesting part of the reply starts with a '#' character */

	ptr = reply;
	while ( *ptr != '#' && ptr < reply + length )
		ptr++;

	/* If no '#' could be found we're in deep trouble... */

	if ( *ptr != '#' )
	{
		T_free( reply );
		return FAIL;
	}

	/* The first byte is the number of digits of the next number... */

	ptr++;
	got_count_digits = ( int ) ( *ptr++ - '0' );

	if ( got_count_digits < 1 )         /* paranoia */
	{
		T_free( reply );
		return FAIL;
	}
		
	/* The next number is the number of relevant bytes */

	for ( got_count = 0, i = 0; i < got_count_digits; i++ )
		got_count = got_count * 10 + ( long ) ( *ptr++ - '0' );

	/* The number of bytes we got as the reply must be as long as we asked for
       - another symptom of paranoia ;-) */

	if ( got_count != params->length )
	{
		T_free( reply );
		return FAIL;
	}

	/* Now check that the bytes received show the correct state */

	for ( i = 0; i < got_count; i++ )
		if ( *ptr++ != state_char )
			break;                     /* whoops, an error ! */

	T_free( reply );

	/* If we got to the end of the list of bytes everything is ok */

	return ( i == got_count ? OK : FAIL );
}
