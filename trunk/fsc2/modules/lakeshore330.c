/*
  $Id$
*/


#include "fsc2.h"
#include "gpib_if.h"


#define DEVICE_NAME "LAKESHORE330"    /* compare entry in /etc/gpib.conf ! */

#define SAMPLE_CHANNEL_A       0
#define SAMPLE_CHANNEL_B       1
#define DEFAULT_SAMPLE_CHANNEL SAMPLE_CHANNEL_B

#define LOCK_STATE_LOCAL       0
#define LOCK_STATE_REMOTE      1
#define LOCK_STATE_REMOTE_LLO  2


int lakeshore330_init_hook( void );
int lakeshore330_exp_hook( void );
int lakeshore330_end_of_exp_hook( void );
void lakeshore330_exit_hook( void );



Var *temp_contr_temperature( Var *v );
Var *temp_contr_sample_channel( Var *v );
Var *temp_contr_lock_keyboard( Var *v );


static bool lakeshore330_init( const char *name );
static double lakeshore330_sens_data( void );
static long lakeshore330_sample_channel( long channel );
static void lakeshore330_lock( int state );
static void lakeshore330_gpib_failure( void );




typedef struct {
	int device;
	int lock_state;
	int sample_channel;
} LAKESHORE330;


static LAKESHORE330 lakeshore330;


/**********************************************************/
/*                                                        */
/*                  hook functions                        */
/*                                                        */
/**********************************************************/


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_init_hook( void )
{
	Cur_Func = "lakeshore330_init_hook";

	need_GPIB = SET;
	lakeshore330.device = -1;
	lakeshore330.lock_state = LOCK_STATE_REMOTE_LLO;
	lakeshore330.sample_channel = DEFAULT_SAMPLE_CHANNEL;

	Cur_Func = NULL;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_exp_hook( void )
{
	Cur_Func = "lakeshore330_exp_hook";
	if ( ! lakeshore330_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "%s: Initialization of device failed.\n", DEVICE_NAME );
		THROW( EXCEPTION );
	}
	Cur_Func = NULL;
	return 1;
}


/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

int lakeshore330_end_of_exp_hook( void )
{
	Cur_Func = "lakeshore330_end_of_exp_hook";
	lakeshore330_lock( 0 );
	Cur_Func = NULL;
	return 1;
}


/**********************************************************/
/*                                                        */
/*          EDL functions                                 */
/*                                                        */
/**********************************************************/


/*---------------------------------------------*/
/* Returns temperature reading from controller */
/*---------------------------------------------*/

Var *temp_contr_temperature( Var *v )
{
	v = v;

	if ( TEST_RUN )
		return vars_push( FLOAT_VAR, 123.45 );
	else
		return vars_push( FLOAT_VAR, lakeshore330_sens_data( ) );
}


/*-----------------------------------------------------------------*/
/* Sets or returns sample channel (fuction returns either 1 or 2   */
/* for channel A or B and accepts 1 or 2 or the strings "A" or "B" */
/* as input arguments).                                            */
/*-----------------------------------------------------------------*/

Var *temp_contr_sample_channel( Var *v )
{
	long channel;

	if ( v == NULL )
		return vars_push( INT_VAR, lakeshore330.sample_channel );
	else
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

	if ( v->type & ( INT_VAR || FLOAT_VAR ) )
	{
		if ( v->type == INT_VAR )
			channel = v->val.lval - 1;
		else
		{
			eprint( WARN, "%s:%ld: %s: Float value used as channel channel "
					"number in %s().\n", Fname, Lc, DEVICE_NAME, Cur_Func );
			channel = lround( v->val.dval ) - 1;
		}

		if ( channel != SAMPLE_CHANNEL_A && channel != SAMPLE_CHANNEL_B )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid sample channel number (%ld) "
					"in %s().\n", Fname, Lc, DEVICE_NAME, channel, Cur_Func );
			THROW( EXCEPTION );
		}
	}
	else
	{
		if ( ( *v->val.sptr != 'A' && *v->val.sptr != 'B' ) ||
			 strlen( v->val.sptr ) != 1 )
		{
			eprint( FATAL, "%s:%ld: %s: Invalid sample channel (\"%s\") in "
					"%s().\n", Fname, Lc, DEVICE_NAME, v->val.sptr, Cur_Func );
			THROW( EXCEPTION );
		}
		channel = ( long ) ( *v->val.sptr - 'A' );
	}

	if ( TEST_RUN )
	{
		lakeshore330.sample_channel = channel;
		return vars_push( INT_VAR, channel + 1 );
	}
	return vars_push( INT_VAR, lakeshore330_sample_channel( channel ) + 1 );
}


/*---------------------------------------------------------*/
/* If called with a non-zero argument the keyboard becomes */
/* unlocked during an experiment.                          */
/*---------------------------------------------------------*/

Var *temp_contr_lock_keyboard( Var *v )
{
	int lock;


	if ( v == NULL )
		lock = LOCK_STATE_REMOTE_LLO;
	else
	{
		vars_check( v, INT_VAR | FLOAT_VAR | STR_VAR );

		if ( v->type == INT_VAR )
			lock = v->val.lval == 0 ?
				LOCK_STATE_REMOTE_LLO : LOCK_STATE_REMOTE;
		else if ( v->type == FLOAT_VAR )
			lock = v->val.dval == 0.0 ?
				LOCK_STATE_REMOTE_LLO : LOCK_STATE_REMOTE;
		else
		{
			if ( ! strcasecmp( v->val.sptr, "OFF" ) )
				lock = LOCK_STATE_REMOTE;
			else if ( ! strcasecmp( v->val.sptr, "ON" ) )
				lock = LOCK_STATE_REMOTE_LLO;
			else
			{
				eprint( FATAL, "%s:%d: %s: Invalid argument in call of "
						"function %s().\n", Fname, Lc, DEVICE_NAME, Cur_Func );
				THROW( EXCEPTION );
			}
		}
	}

	if ( ! TEST_RUN )
		lakeshore330_lock( lock - 1 );
	return vars_push( INT_VAR, lock == LOCK_STATE_REMOTE_LLO ? 1 : 0 );
}


/**********************************************************/
/*                                                        */
/*       Internal (not-exported) functions                */
/*                                                        */
/**********************************************************/

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/

static bool lakeshore330_init( const char *name )
{
	char buf[ 20 ];
	long len = 20;



	/* Initialize GPIB communication with the temperature controller */

	if ( gpib_init_device( name, &lakeshore330.device ) == FAILURE )
		return FAIL;

	/* Set end of EOS character to '\n' */

	if ( gpib_write( lakeshore330.device, "TERM 2\r\n", 8 ) == FAILURE ||
		 gpib_write( lakeshore330.device, "END 0\n", 6 ) == FAILURE )
		return FAIL;

	if ( gpib_write( lakeshore330.device, "*STB?\n", 6 ) == FAILURE ||
		 gpib_read( lakeshore330.device, buf, &len ) == FAILURE )
		return FAIL;

	/* Set default sample channel */

	sprintf(buf, "SCHN %c\n", ( char ) ( lakeshore330.sample_channel + 'A' ) );
	if ( gpib_write( lakeshore330.device, buf, strlen( buf ) ) == FAILURE )
		return FAIL;
	usleep( 500000 );

	/* Switch device to remote state with local lockout */

	if ( gpib_write( lakeshore330.device, "MODE 2\n", 7 ) == FAILURE )
		return FAIL;

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static double lakeshore330_sens_data( void )
{
	char buf[ 50 ];
	long len = 50;
	double temp;


	if ( gpib_write( lakeshore330.device, "SDAT?\n", 6 ) == FAILURE ||
		 gpib_read( lakeshore330.device, buf, &len ) == FAILURE )
		lakeshore330_gpib_failure( );

	if ( *buf != '-' && *buf != '+' && ! isdigit( *buf ) )
	{
		eprint( FATAL, "%s:%ld: %s: Error reading temperature in %s().\n",
				Fname, Lc, DEVICE_NAME, Cur_Func );
		THROW( EXCEPTION );
	}

	sscanf( buf, "%lf", &temp );
	return temp;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static long lakeshore330_sample_channel( long channel )
{
	char buf[ 20 ];


	assert( channel == SAMPLE_CHANNEL_A || channel == SAMPLE_CHANNEL_B );

	sprintf( buf, "SCHN %c\n", ( char ) ( channel + 'A' ) );
	if ( gpib_write( lakeshore330.device, buf, strlen( buf ) ) == FAILURE )
		lakeshore330_gpib_failure( );

	usleep( 500000);
	return lakeshore330.sample_channel = channel;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void lakeshore330_lock( int state )
{
	char cmd[ 20 ];


	assert ( state >= LOCK_STATE_LOCAL && state <= LOCK_STATE_REMOTE_LLO );

	sprintf( cmd, "MODE %d\n", state );
	if ( gpib_write( lakeshore330.device, cmd, strlen( cmd ) ) == FAILURE )
		lakeshore330_gpib_failure( );

	lakeshore330.lock_state = state;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void lakeshore330_gpib_failure( void )
{
	eprint( FATAL, "%s: Communication with device failed.\n", DEVICE_NAME );
	THROW( EXCEPTION );
}
