/*
  $Id$
*/



#include "fsc2.h"




/* exported functions */

int s_band_init_hook( void );
int s_band_test_hook( void );
int s_band_exp_hook( void );
void s_band_exit_hook( void );



#define sign( x ) ( ( ( x ) >= 0.0 ) ? 1.0 : -1.0 )


enum {
	   SERIAL_INIT,
	   SERIAL_TRIGGER,
	   SERIAL_VOLTAGE,
	   SERIAL_EXIT,
};

/* MAGNET_ZERO_STEP is the data to be send to the magnet to achieve a sweep
   speed of exactly zero - which is 0x800 minus `half a bit' :-)
   MAGNET_ZERO_STEP and MAGNET_MAX_STEP must always add up to something not
   more than 0xFFF and MAGNET_ZERO_STEP minus MAGNET_MAX_STEP must always be
   zero or larger ! */

#define MAGNET_ZERO_STEP  ( 0x800 - 0.5 )  /* data for zero sweep speed */
#define MAGNET_MAX_STEP   ( 0x7FF + 0.5 )  /* maximum sweep speed setting */

#define MAGNET_TEST_STEPS 0x10   /* number of steps to do in test */
#define MAGNET_TEST_WIDTH 0x400  /* sweep speed setting for test */
#define MAGNET_MAX_TRIES  3      /* number of retries after failure of magnet
                                    field convergence to target point */

/* definitions used for serial port access */

#define SERIAL_BAUDRATE B1200        /* baud rate of field controller */
#define SERIAL_PORT     "/dev/ttyS1" /* serial port device file */
#define SERIAL_TIME     50000        /* 50 ms */

#define E2_US           100000       /* 100 ms, used in calls of usleep() */


#define S_BAND_MIN_FIELD_STEP              1.5e-3
#define S_BAND_WITH_ER035M_MIN_FIELD       460
#define S_BAND_WITH_ER035M_MAX_FIELD       2390
#define S_BAND_WITH_ER035M_MIN_FIELD       0
#define S_BAND_WITH_XXXXXX_MAX_FIELD       9000


typedef struct
{
	double field;          /* the start field given by the user */
	double field_step;     /* the field steps to be used */

	bool is_field;
	bool is_field_step;

	double mini_step;      /* the smallest possible field step */

	double target_field;
	double act_field;

	double meas_field;

	double step;           /* the current step width (in bits) */
	int int_step;
	int flags;

    int fd;                           
    struct termios old_tio,
                   new_tio;
} Magnet;



static Magnet magnet = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
						 0, 0, -1, NULL, NULL };



int s_band_init_hook( void )
{
	magnet.is_field = UNSET;
	magnet.is_field_step = UNSET;

	return 1;
}


int s_band_test_hook( void )
{
	return 1;
}


int s_band_exp_hook( void )
{
	Var *func;


	/* Check that the fuctions exported by the field meter driver can be
	   accessed */

	if ( ( func = func_get( "find_field" ) ) == NULL )
	{
		eprint( FATAL, "Magnet driver: Can't find functions for field "
				"measurements.\n" );
		THROW( EXCEPTION );
	}
	vars_pop( func );

	if ( ( func = func_get( "field_meter_wait" ) ) == NULL )
	{
		eprint( FATAL, "Magnet driver: Can't find functions for field "
				"measurements.\n" );
		THROW( EXCEPTION );
	}
	vars_pop( func );

	/* Try to initialize the magnet poser supply controller */

	if ( ! magnet_init( ) )
	{
		eprint( FATAL, "Can't access the S-band magnet power supply.\n" );
		THROW( EXCEPTION );
	}

	return 1;
}


void s_band_exit_hook( void )
{
	if ( magnet.fd >= 0 )
		magnet_do( SERIAL_EXIT ) 

	magnet.fd = -1;
}




void magnet_setup( Var *v )
{
	/* check that both variables are reasonable */

	vars_check( v, INT_VAR | FLOAT_VAR );
	vars_check( v->next, INT_VAR | FLOAT_VAR );

	if ( exist_device( "er035m" ) )
	{
		if ( VAL( v ) < S_BAND_WITH_ER035M_MIN_FIELD )
		{
			eprint( FATAL, "Start field too low for Bruker ER035M gaussmeter, "
					"minimum is %d G.\n",
					( int ) S_BAND_WITH_ER035M_MIN_FIELD );
			THROW( EXCEPTION );
		}
        
		if ( magnet.field > S_BAND_WITH_ER035M_MAX_FIELD )
		{
			eprint( FATAL, "Start field too high for Bruker ER035M "
					"gaussmeter, maximum is %d G.\n",
					( int ) S_BAND_WITH_ER035M_MAX_FIELD );
			THROW( EXCEPTION );
		}
	}










static bool magnet_goto_field( void );
static bool magnet_goto_field_rec( int rec );



/* The sweep of the magnet is done by setting a voltage for a certain time
   (to be adjusted manually on the front panel but which should be left at
   the current setting of 50 ms) to the internal sweep circuit.  There are
   two types of data to be sent to the home build interface card: First, the
   voltage to be set and, second, a trigger to really set the voltage.

   1. The DAC is a 12-bit converter, thus two bytes have to be sent to the
      interface card. The first byte must have bit 6 set to tell the inter-
	  face card that this is the first byte. In bit 0 to 3 of the first byte
	  sent the upper 4 bits of the 12-bit data are encoded, while bit 4
	  contains the next lower bit (bit 7) of the 12-data word to be sent.
	  In the second byte sent to the interface card bit 7 has to be set to
	  tell the card that this is the second byte, followed by the remaining
	  lower 7 bits of the data. Thus in order to send the 12-bit data

                       xxxx yzzz zzzz

	  the 2-byte sequence to be sent to the interface card is (binary)

                  1.   010y xxxx
				  2.   1zzz zzzz

  2. To trigger the output of the voltage bit 5 of the byte sent to the
     interface card has to be set, while bit 6 and 7 have to be zeros - all
	 remaining bits don't matter. So, the (binary) trigger command byte is

                       0010 0000

  3. The maximum positive step width is achieved by setting the DAC to
     0x0, the maximum negative step width by setting it to 0xFFF.

  The sweep according to the data send to the magnet power supply will
  last for the time adjusted on the controllers front panel (in ms).
  This time is currently set to 50 ms and shouldn't be changed without
  good reasons.

*/



/*--------------------------------------------------------------------------*/
/* magnet_init() first initialises the serial interface and then tries to   */
/* figure out what's the current minimal step size is for the magnet - this */
/* is necessary for every new sweep since the user can adjust the step size */
/* by setting the sweep rate on the magnets front panel (s/he also might    */
/* change the time steps but lets hope he doesn't since there's no way to   */
/* find out about it...). We also have to make sure that the setting at the */
/* front panel is the maximum setting of 6666 Oe/min. Finally we try to go  */
/* to the start field.                                                      */
/*--------------------------------------------------------------------------*/

bool magnet_init( void )
{
	double start_field;
	int i;
	Var *v;


	/* First step: Initialisation of the serial interface */

	if ( ! magnet_do( SERIAL_INIT ) )
		return( FAIL );

	/* Next step: We do MAGNET_TEST_STEPS steps with a step width of
	   MAGNET_TEST_WIDTH. Then we measure the field to determine the
	   current/field ratio */

try_again:

	vars_call( func_get( "field_meter_wait" ) );

	v = vars_call( func_get( "find_field" ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	start_field = magnet.meas_field;
	magnet.step = MAGNET_TEST_WIDTH;
	magnet_do( SERIAL_VOLTAGE );

	for ( i = 0; i < MAGNET_TEST_STEPS; ++i )
	{
		magnet_do( SERIAL_TRIGGER );
		vars_call( func_get( "field_meter_wait" ) );
	}

	vars_call( func_get( "field_meter_wait" ) );

	v = vars_call( func_get( "find_field" ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	/* calculate the smallest possible step width (in field units) */

	magnet.mini_step = fabs( magnet.meas_field - start_field ) /
		                  ( double ) ( MAGNET_TEST_WIDTH * MAGNET_TEST_STEPS );

	/* Now lets do the same, just in the opposite direction to increase
	   accuracy */

	start_field = magnet.meas_field;
	magnet.step = - MAGNET_TEST_WIDTH;
	magnet_do( SERIAL_VOLTAGE );

	for ( i = 0; i < MAGNET_TEST_STEPS; ++i )
	{
		magnet_do( SERIAL_TRIGGER );
		vars_call( func_get( "field_meter_wait" ) );
	}

	vars_call( func_get( "field_meter_wait" ) );

	v = vars_call( func_get( "find_field" ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	magnet.mini_step -= ( magnet.meas_field - start_field ) /
		                  ( double ) ( MAGNET_TEST_WIDTH * MAGNET_TEST_STEPS );
	magnet.mini_step *= 0.5;

	/* Check that the sweep speed selector on the magnets front panel is set
	   to the maximum, i.e. 6666 Oe/min - otherwise ask user to change the
	   setting and try again */

	if ( magnet.mini_step < 0.00074 )
	{
		if ( 1 == fl_show_choice( "Please set sweep speed on magnet front",
								  "panel to maximum value of 6666 Oe/min.",
								  "Also make sure remote control is enabled !",
								  2, "Abort", "Done", "", 3 ) )
			return( FAIL );
		else
			goto try_again;
	}

	/* Finally using this ratio we go to the start field */

	return( magnet_goto_field( ) );

}


/*--------------------------------------------------------------------------*/
/* This just a wrapper to hide the recursiveness of magnet_goto_field_rec() */
/*--------------------------------------------------------------------------*/

bool magnet_goto_field( void )
{
	if ( ! magnet_goto_field_rec( 0 ) )
		return( FAIL );

	magnet.act_field = magnet.target_field = magnet.field;
	return( OK );
}


bool magnet_goto_field_rec( Magnet int rec )
{
	double mini_steps;
	int steps;
	double remainder;
	int i;
	static double last_diff;  /* field difference in last step */
	static int try;           /* number of steps without conversion */
	Var *v


	/* determine the field between the target field and the current field */

	v = vars_call( func_get( "find_field" ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	/* calculate the number of steps we need using the smallest possible
	   field increment (i.e. 1 bit) */

	mini_steps = ( magnet.field - magnet.meas_field ) / magnet.mini_step;

	/* how many steps need we using the maximum step size and how many
	   steps with the minimum step size remain ? */

	steps = ( int ) floor( fabs( mini_steps ) / MAGNET_MAX_STEP );
	remainder = mini_steps - sign( mini_steps ) * steps * MAGNET_MAX_STEP;

	/* Now do all the steps to reach the target field */

	if ( steps > 0 )
	{
		magnet.step = sign( mini_steps ) * MAGNET_MAX_STEP;
		magnet_do( SERIAL_VOLTAGE );

		for ( i = 0; i < steps; ++i )
			magnet_do( SERIAL_TRIGGER );
	}

	if ( ( magnet.step = remainder ) != 0.0 )
	{
		magnet_do( SERIAL_VOLTAGE );
		magnet_do( SERIAL_TRIGGER );
	}

	/* Finally we check if the field value is reached. If it isn't we do
	   further corrections by calling the routine itself. To avoid running
	   in an endless recursion with the difference between the actual field
	   and the target field becoming larger and larger we allow the
	   difference becoming larger only MAGNET_MAX_TRIES times - after that
	   we have to assume that something has gone wrong (maybe some luser
	   switched the magnet from remote control state? ) */

	vars_call( func_get( "field_meter_wait" ) );

	v = vars_call( func_get( "find_field" ) );
	magnet.meas_field = v->val.dval;
	vars_pop( v );

	if ( rec == 0 )               /* at the very first call of the routine */
	{
		last_diff = HUGE_VAL;
		try = 0;
	}
	else                          /* if we're already in the recursion */
	{                                                       /* got it worse? */
		if ( fabs( magnet.field - magnet.meas_field ) > last_diff )
		{
			if ( ++try >= MAGNET_MAX_TRIES )
				return( FAIL );
		}
		else                                /* difference got smaller */
		{
			try = 0;
		    last_diff = fabs( magnet.field - magnet.meas_field );
		}
	}

	/* Deviation from target field has to be no more than 1 mG otherwise
	   try another (hopefully smaller) step */

	if ( fabs( magnet.field - magnet.meas_field ) >= 0.001 &&
		 ! magnet_goto_field_rec( magnet, 1 ) )
		return( FAIL );

	return( OK );
}



bool magnet_sweep( int dir )
{
	int steps, i;
	double mini_steps;
	double remainder;
	double over_shot;
	

	/* check for unreasonable input */

	if ( abs( dir ) != 1 )
		return( FAIL );

	/* Problem: while there can be arbitrary sweep steps requested by the
	   user, the real sweep steps are discreet. This might sum up to a
	   noticeable error after several sweep steps. So, to increase accuracy,
	   at each sweep step the field that should be reached is calculated
	   (magnet.target_field) as well as the field actually reached by the
	   current sweep step (magnet.act_field). The difference between these
	   values is used as a correction in the next sweep step (and should
	   never be larger than one bit). */

	over_shot = magnet.act_field - magnet.target_field;
   	mini_steps = ( ( double ) dir * magnet.field_step - over_shot ) /
		                                                     magnet.mini_step;

	/* If the target field can be achieved by a single step... */

	if ( fabs( mini_steps ) <= MAGNET_MAX_STEP )
	{
		if ( magnet.step != mini_steps )
		{
			magnet.step = mini_steps;
			magnet_do( SERIAL_VOLTAGE );
		}
		magnet_do( SERIAL_TRIGGER );

		magnet.act_field += ( MAGNET_ZERO_STEP - magnet.int_step )
			                                               * magnet.mini_step;
		magnet.target_field += ( double ) dir * magnet.field_step;
		return( OK );
	}

	/* ...otherwise we need several steps with in MAGNET_MAX_STEP chunks
	   plus a last step for the remainder */
	/* how many steps need we using the maximum step size and how many
	   steps with the minimum step size remain ? */

	steps = ( int ) floor( fabs( mini_steps ) / MAGNET_MAX_STEP );
	remainder = mini_steps - sign( mini_steps ) * steps * MAGNET_MAX_STEP;

	/* Now do all the steps to reach the target field */

	if ( steps > 0 )
	{
		magnet.step = sign( mini_steps ) * MAGNET_MAX_STEP;
		magnet_do( SERIAL_VOLTAGE );

		for ( i = 0; i < steps; ++i )
		{
			magnet_do( SERIAL_TRIGGER );
			magnet.act_field += ( MAGNET_ZERO_STEP - magnet.int_step )
			                                               * magnet.mini_step;
		}
	}

	if ( ( magnet.step = remainder ) != 0.0 )
	{
		magnet_do( SERIAL_VOLTAGE );
		magnet_do( SERIAL_TRIGGER );
	}

	magnet.act_field += ( MAGNET_ZERO_STEP - magnet.int_step )
			                                               * magnet.mini_step;
	magnet.target_field += ( double ) dir * magnet.field_step;

	return( OK );
}



/*---------------------------------------------------------------------------*/
/* This is the most basic routine for controlling the field - there are four */
/* basic commands, i.e. initializing the serial interface, setting a sweep   */
/* voltage, triggering a field sweep and finally resetting the serial inter- */
/* face.                                                                     */
/*---------------------------------------------------------------------------*/

bool magnet_do( int command )
{
	unsigned char data[ 2 ];
	int volt;


	switch ( command )
	{
		case SERIAL_INIT :               /* open and initialize serial port */
			if ( ( magnet.fd =
				  open( SERIAL_PORT, O_WRONLY | O_NOCTTY | O_NONBLOCK ) ) < 0 )
				return( FAIL );

			tcgetattr( magnet.fd, &magnet.old_tio );
			memcpy( ( void * ) &magnet.new_tio, ( void * ) &magnet.old_tio,
					sizeof( struct termios ) );
			magnet.new_tio.c_cflag = SERIAL_BAUDRATE | CS8 | CRTSCTS;
			tcflush( magnet.fd, TCIFLUSH );
			tcsetattr( magnet.fd, TCSANOW, &magnet.new_tio );

		case SERIAL_TRIGGER :                 /* send trigger pattern */
			data[ 0 ] = 0x20;
			write( magnet.fd, ( void * ) &data, 1 );
			usleep( SERIAL_TIME );
			break;

		case SERIAL_VOLTAGE :                 /* send voltage data pattern */
			magnet.int_step = volt = rnd( MAGNET_ZERO_STEP - magnet.step );
		    data[ 0 ] = ( unsigned char ) 
				( 0x40 | ( ( volt >> 8 ) & 0xF ) | ( ( volt >> 3 ) & 0x10 ) );
			data[ 1 ] = ( unsigned char ) ( 0x80 | ( volt & 0x07F ) );
			write( magnet.fd, ( void * ) &data, 2 );
			break;

		case SERIAL_EXIT :                    /* reset and close serial port */
			tcflush( magnet.fd, TCIFLUSH );
			tcsetattr( magnet.fd, TCSANOW, &magnet.old_tio );
			close( magnet.fd );
			break;

		default :
		    return( FAIL );
	}

	return( OK );
}
