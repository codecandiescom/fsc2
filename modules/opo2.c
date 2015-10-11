/*  
 *   (C) 2010 FU Berlin, Dept. of Physics, AG Bittl
 *
 *   Author: Bjoern Beckmann
 *   Update: Jens Thoms Toerring (2015)
 *
 *   This file is distributed with fsc2 with permission by Prof. R. Bittl.
 *
 *   OPTA-OPO module 
 *   SMD-500
 *
 *   Version 2
 *   10/2015
 */


#include "fsc2_module.h"
#include "opo.conf"
#include "serial.h"


const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;

/*
  Define the length of array storing wavelength/motor positions from the
  BBO_OPO.CAL datafile. Length depends on

  (CalLambdaEnd - CalLambdaBegin) / (smallest Stepwidth in opo_calibration) + 1

  because the lambdaFromSteps and stepsFromLambda functions need an additional
  field
*/

#define CAL_LAMBDA_LENGTH 340

#define SERIAL_BAUDRATE B19200
#define CalBegin 390.
#define CalEnd   730.


int opo_init_hook( void );
int opo_exp_hook( void );
int opo_end_of_exp_hook( void );


Var_T * opo_name( Var_T * v );
Var_T * opo_initializeDevice( Var_T * v );
Var_T * opo_goTo( Var_T * v );
Var_T * opo_scan( Var_T * v );
Var_T * opo_setStepperFrequency( Var_T * v );
Var_T * opo_originSearch( Var_T * v );
Var_T * opo_status( Var_T * v );
Var_T * opo_stop( Var_T * v );
Var_T * opo_idlerFromSignal( Var_T * v );
Var_T * opo_signalFromIdler( Var_T * v );
Var_T * opo_calibration( Var_T * v );
Var_T * opo_setOffset( Var_T * v );
Var_T * opo_goToInteractive( Var_T * v );


static
struct OPO
{
    int        handle;
    double     CalLambda[ CAL_LAMBDA_LENGTH + 1 ];
    int        CalMotorPos[ CAL_LAMBDA_LENGTH + 1 ];
    double     CalLambdaBegin;
    double     CalLambdaEnd;
    int        CalStepBegin;
    int        CalStepEnd;
    int        LengthOfMotorPosArray;
} opo;


static void opo_sendCommand( const unsigned char * c );
static void opo_setAcceleration( unsigned int StepperStartFrequenz,
								 unsigned int StepperMaxFrequenz,
								 unsigned int StepperAccLength);
static void opo_stepperSetAbsPosition( unsigned int ParAbsolutePosition );
static void opo_stepperSetAbsVarFreq( unsigned int ParAbsolutePosition,
									  unsigned int GoToSpeed );
static void opo_autoPromptEnable( void );
static void opo_autoPromptDisable( void );
static void opo_triggerCount( unsigned int events );
static void opo_specialWait( void );
static void opo_promptOnTriggerEnable( void );
static void opo_promptOnTriggerDisable( void );
static void opo_deceleratingStop( void );
#if 0
static void opo_requestPrompt( void );
#endif
static int opo_stepsFromLambda( double ParLambda );
static double opo_lambdaFromSteps( int steps );
#if 0
static double opo_signalFromIdlerIntern( double idler );
#endif
static double opo_idlerFromSignalIntern( double opo_signal );
static int opo_reachedPosition( int stepPosition,
								int b );
static void opo_failure( const char * info );
static void opo_checkCorrectRange( unsigned int motorStatus,
								   int          stepPositionErg,
								   int          b );
static const unsigned char * opo_read( int us_delay );



/*            Hook functions                 */

/*------------------------------------------------------*
 * Function called when the module has just been loaded
 *------------------------------------------------------*/

int
opo_init_hook( void )
{
	opo.handle = -1;
    opo.handle = fsc2_request_serial_port( SERIAL_PORT, DEVICE_NAME );
    
    FILE * fp = fopen( BBO_OPO_PATH, "r" );
    if ( fp == NULL )
	{
		print( FATAL, "Can't open calibration file '%s' for reading.\n",
			   BBO_OPO_PATH );
		return 0;
	}

    int cnt,
        g = 0;
    do
	{
		g++;
        cnt = fscanf( fp, "%d %lf %d", &opo.LengthOfMotorPosArray,
                      opo.CalLambda + g, opo.CalMotorPos + g );
	} while ( cnt == 3 && g < CAL_LAMBDA_LENGTH );

    fclose( fp );

    if ( cnt != EOF && cnt != 3 )
	{
		print( FATAL, "Invalid content of calibration file '%s' at line %d.\n",
			   BBO_OPO_PATH, g );
		return 0;
	}

	if ( cnt == 3 )
		print( WARN, "More than %d entries in '%s', discarding rest of file.\n",
			   CAL_LAMBDA_LENGTH, BBO_OPO_PATH );

    opo.CalLambdaBegin = opo.CalLambda[ 1 ];
    opo.CalLambdaEnd   = opo.CalLambda[ g - 1 ];
    opo.CalStepBegin   = opo.CalMotorPos[ 1 ];
    opo.CalStepEnd     = opo.CalMotorPos[ g - 1 ];

    return 1;
}


/*-----------------------------------------------*
 * Function called at the start of an experiment
 *-----------------------------------------------*/

int
opo_exp_hook( void )
{   
    /*
      serial communication parameters: 19200 Baud, no handshake, 8 data bits,
      1 stop bit, no parity 
    */

    struct termios * tio =
		          fsc2_serial_open( opo.handle,
									O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK );
	if ( ! tio )
	{
		print( FATAL, "Failed to initialize connection to device.\n" );
		return 0;
	}

    tio->c_cflag  = 0; 
    tio->c_cflag |= CS8;
    tio->c_cflag |= CLOCAL | CREAD;

    tio->c_iflag = IGNBRK;
    tio->c_oflag = 0;
    tio->c_lflag = 0;

    cfsetispeed( tio, SERIAL_BAUDRATE );
    cfsetospeed( tio, SERIAL_BAUDRATE );

    fsc2_tcflush( opo.handle, TCIOFLUSH );
    fsc2_tcsetattr( opo.handle, TCSANOW, tio );

    opo_autoPromptEnable( );
    opo_setAcceleration( 3000, 6000, 4000 );

    return 1;
}


/*---------------------------------------------*
 * Function called at the end of an experiment
 *---------------------------------------------*/

int
opo_end_of_exp_hook( void )
{
	if ( opo.handle >= 0 )
		fsc2_serial_close( opo.handle );
    return 1;
}



/* EDL FUNCTIONS */

/*------------------------------------------------------*
 *------------------------------------------------------*/

Var_T *
opo_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

Var_T *
opo_initializeDevice( Var_T * v  UNUSED_ARG )
{
    unsigned char cmd_str[ 10 ] = { 0x01  /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );

    opo_setAcceleration( 3000, 12000, 4000 );

    return opo_originSearch( NULL );
}


/*------------------------------------------------------*
  opo_goTo expects two parameters:
  double goToPos as Signal (between min/max-value from BBO_OPO.cal)
  double goToSpeed (between 0.1 - 20.)
 *------------------------------------------------------*/

Var_T *
opo_goTo( Var_T * v )
{
    double goToPos   = get_double( v, NULL );
    double goToSpeed = get_double( vars_pop( v ), NULL );

    if ( goToPos < opo.CalLambdaBegin || goToPos > opo.CalLambdaEnd )
    {
        print( FATAL, "Wavelength out of range: %f - %f.\n",
               opo.CalLambdaBegin, opo.CalLambdaEnd );
        THROW( EXCEPTION );
    }

    if ( goToSpeed < 0.1 || goToSpeed > 20.0 )
	{
		print( FATAL, "Speed out of range: 0.1 - 20.\n" );
		THROW( EXCEPTION );
    }

    int st = opo_stepsFromLambda( goToPos );
    opo_stepperSetAbsVarFreq( st, irnd( 400 * goToSpeed ) );
    opo_reachedPosition( st, 1 );

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
  opo_scan expects 6 parameters:
    double scanStart  : as Signal (between min/max-value from BBO_OPO.cal)
    double scanStop   : as Signal (between min/max-value from BBO_OPO.cal)
    double scanSpeed  : between 0.1 - 20.
    bool   scanMode   : 0 or "off" for continuous mode, 1 or "on" for
                        triggered mode 
    int    scanEvents : between 1 and 10
    double increment  : between 1. and 10.
 *------------------------------------------------------*/

Var_T *
opo_scan( Var_T * v )
{
    double scanStart = get_double( v, NULL );
    if ( scanStart < opo.CalLambdaBegin || scanStart > opo.CalLambdaEnd )
    {
        print( FATAL, "scanStart out of range: %f - %f\n",
               opo.CalLambdaBegin, opo.CalLambdaEnd );
        THROW( EXCEPTION );
    }

    double scanStop  = get_double( v = vars_pop( v ), NULL );
    if ( scanStop < opo.CalLambdaBegin || scanStop > opo.CalLambdaEnd )
    {
        print( FATAL, "scanStop out of range: %f - %f\n",
               opo.CalLambdaBegin, opo.CalLambdaEnd );
        THROW( EXCEPTION );
    }

    double scanSpeed = get_double( v = vars_pop( v ), NULL );
    if ( scanSpeed < 0.1 || scanSpeed > 20.0 )
	{
		print( FATAL, "Scan speed out of range: 0.1 - 20.\n");
		THROW( EXCEPTION );
    }

    bool scanMode    = get_boolean( v = vars_pop( v ) );

    long scanEvents  = get_strict_long( v = vars_pop( v ), "scan events" );
    if ( scanEvents < 1 || scanEvents > 10 )
	{
		print( FATAL, "Scan events out of range: 1-10.\n" );
		THROW( EXCEPTION );
    }

    double increment = get_double( v = vars_pop( v ), NULL );
    if ( increment < 1. || increment > 10. )
	{
		print( FATAL, "Increment value out of range: 1.-10.\n" );
		THROW( EXCEPTION );
    }

    opo_setAcceleration( 3000, 12000, 1000 );

    if ( FSC2_MODE == EXPERIMENT )
        print( NO_ERROR, "opo moves to the scan start point.\n" );

    int start = opo_stepsFromLambda( scanStart );
    opo_stepperSetAbsPosition( start );
    opo_reachedPosition( start, 1 );

    if ( FSC2_MODE == EXPERIMENT )
        print( NO_ERROR, "opo reached the scan start point.\n" );

    if ( ! scanMode )
    {
        if ( FSC2_MODE == EXPERIMENT )
            print( NO_ERROR, "opo starts continued scan.\n" );

        int st = opo_stepsFromLambda( scanStop );     
        opo_stepperSetAbsVarFreq( st, irnd( 140 * scanSpeed ) );
        opo_reachedPosition( st, 1 );

        if ( FSC2_MODE == EXPERIMENT )
            print( NO_ERROR, "opo reached the scan end point.\n" );
    }
    else
    {
        // triggered mode

        opo_autoPromptDisable( );        
        opo_triggerCount( scanEvents );
        opo_promptOnTriggerEnable( );
        opo_specialWait( );

        if ( FSC2_MODE == EXPERIMENT )
        {
            // "empty" serial interface

			const unsigned char * cmd = opo_read( -1 );
            print( NO_ERROR, "opo starts triggered scan.\n" );

            while ( true )
            {
				cmd = opo_read( -1 );
                if ( cmd[ 0 ] == '[' && cmd[ 13 ] == ']' )
                {
                    unsigned int motorStatus = cmd[ 3 ] & 0x1f;
                    if ( motorStatus == 2 )
                    {
						int steps =   ( cmd[ 6 ] * 256 + cmd[ 5 ] ) * 256
							        + cmd[ 4 ];

                        opo_checkCorrectRange( motorStatus, steps, 1 );
                        double lambda = opo_lambdaFromSteps( steps );
            
                        if ( scanStart <= scanStop )
                            lambda = lambda + increment;
                        else
                            lambda = lambda - increment;            
                        
                        if (    lambda < opo.CalLambdaBegin
                             || lambda > opo.CalLambdaEnd )
                        {
                            print( FATAL, "opo is out of calibrated "
								   "range: %f - %f\n",
								   opo.CalLambdaBegin, opo.CalLambdaEnd );
                            THROW( EXCEPTION );
                        }
                        
                        int etap = opo_stepsFromLambda( lambda );
                        opo_stepperSetAbsPosition( etap );

						// don't use opo_reachedPosition(etap), because
						// autoPrompt is disabled at this state

                        print( NO_ERROR, "current position (Signal): %f nm\n",
                               lambda );

                        if (    (    scanStart <= scanStop
                                  && lambda + increment <= scanStop )
                             || (    scanStart > scanStop
                                  && lambda - increment >= scanStop ) )
                            opo_specialWait( );
                        else
                        {
                            opo_promptOnTriggerDisable( );
                            opo_autoPromptEnable( );

                            print( NO_ERROR, "opo reached the scan end "
								   "point.\n" );
                            break;
                        }
                    } // motorStatus == 2
                }
            } // while
        } // EXPMODE
    } // else ( ! scanMode )        

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
   opo_setStepperFrequency expects one parameter:
   int stepperMaxFrequency (between 200 and 6000)

   -sets default stepper speed
 *------------------------------------------------------*/

Var_T *
opo_setStepperFrequency( Var_T * v )
{
    int stepperMaxFrequency = irnd( get_double( v, NULL ) );
    if ( stepperMaxFrequency < 200 || stepperMaxFrequency > 6000 )
	{
		print( FATAL, "Maximum stepper frequency out of range: 200 - 6000.\n");
		THROW( EXCEPTION );
	}

    int stepperStartFrequency  = irnd( stepperMaxFrequency / 2 );
    int stepperAccLength = irnd( stepperMaxFrequency * 2.0 / 3.0 );

    opo_setAcceleration( stepperStartFrequency, stepperMaxFrequency,
                         stepperAccLength );

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

Var_T *
opo_originSearch( Var_T * v  UNUSED_ARG )
{
    opo_autoPromptDisable( );

    unsigned char cmd_str[ 10 ] = { 0x8, 1, 2, 1  /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    while ( true )
    {
		const unsigned char * cmd = opo_read( -1 );
        if ( cmd[ 0 ] == '[' && cmd[ 13 ] == ']' )
        {
			unsigned int errorB      = cmd[ 1 ] & 0x0f;
			unsigned int contrStatus = cmd[ 2 ] & 0x03;
            unsigned int motorStatus = cmd[ 3 ] & 0x1f;

			int steps = ( cmd[ 6 ] * 256 + cmd[ 5 ] ) * 256 + cmd[ 4 ];

            print( NO_ERROR, "current step position: %d, motor status: %u, "
                   "error: %u, status controller: %u\n", steps, motorStatus,
				   errorB, contrStatus );

            if ( motorStatus & 4 )
            {
                opo_autoPromptEnable( );

                if ( steps == 0 )
                {
                    int st = opo_stepsFromLambda( 420.0 );     
                    opo_stepperSetAbsVarFreq( st, 4000 );
                    opo_reachedPosition( st, 2 );
                    print( NO_ERROR, "Origin Search finished. Current "
                           "position (Signal): 420 nm.\n" );
					break;
                }
            }
        }
    }

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

Var_T *
opo_status( Var_T * v  UNUSED_ARG )
{
    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

	const unsigned char * cmd = opo_read( 5000000 );
    if ( cmd[ 0 ] == '[' && cmd[ 13 ] == ']' )
    {
        unsigned int errorB      = cmd[ 1 ] & 0x0f;
        unsigned int contrStatus = cmd[ 2 ] & 0x03;
        unsigned int motorStatus = cmd[ 3 ] & 0x1f;

		int position = ( cmd[ 6 ] * 256 + cmd[ 5 ] ) * 256 + cmd[ 4 ];

        opo_checkCorrectRange( motorStatus, position, 1 );

        print( NO_ERROR, "current position (Signal): %f nm, stepper "
               "position: %d, motor status: %u, error: %u, "
               "contrl.tatus: %u\n", opo_lambdaFromSteps( position ),
               position, motorStatus, errorB, contrStatus );
    }

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

Var_T *
opo_stop( Var_T * v  UNUSED_ARG )
{
    opo_deceleratingStop( );
    opo_promptOnTriggerDisable( );
    opo_autoPromptEnable( );

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
   Parameter: double, signal in nm
 *------------------------------------------------------*/

Var_T *
opo_idlerFromSignal( Var_T * v )
{
    double opo_signal = get_double( v, NULL );

    if ( opo_signal < opo.CalLambdaBegin || opo_signal > opo.CalLambdaEnd )
    {
        print( FATAL, "parameter out of range: %f - %f\n",
               opo.CalLambdaBegin, opo.CalLambdaEnd );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, 1.0 / ( 1.0 / 354.767 - 1.0 / opo_signal ) );

}


/*------------------------------------------------------*
   Parameter: double, idler in nm
 *------------------------------------------------------*/

Var_T *
opo_signalFromIdler( Var_T * v )
{
    double opo_idler = get_double( v, NULL );
    if (    opo_idler > opo_idlerFromSignalIntern( opo.CalLambdaBegin )
         || opo_idler < opo_idlerFromSignalIntern( opo.CalLambdaEnd ) )
    {
        print( FATAL, "parameter out of range: %f - %f\n",
               opo_idlerFromSignalIntern( opo.CalLambdaEnd ),
               opo_idlerFromSignalIntern( opo.CalLambdaBegin ) );
        THROW( EXCEPTION );
    }

    return vars_push( FLOAT_VAR, 1.0 / ( 1.0 / 354.767 - 1.0 / opo_idler ) );
}


/*------------------------------------------------------*
    double begin; start point in nm (must be >= CalBegin)
    double end; end point in nm (must be <= CalEnd)
    double step; difference between two points in nm (range 1. - 50.)
    
    user input:     "ok" -> calibrated position is ok; calibrate next step
                            position
             integer num -> opo moves to "current step position" + num
                            (f.e. -100, f.e. 20)

    care about write permissions of BBO_OPO.cal, u.c. u should start fsc2
    with root rights
 *------------------------------------------------------*/

Var_T *
opo_calibration( Var_T * v )
{
    double begin = get_double( v, NULL );
    double end   = get_double( v = vars_pop( v ), NULL );

    if ( begin < CalBegin || end > CalEnd )
    {
        print( FATAL, "Begin calibration position < %f  or end calibration "
			   "position > %f.\n", CalBegin, CalEnd );
        THROW( EXCEPTION );
    }

    if ( begin > 420.0 || end < 420.0 )
	{
		print( FATAL, "Begin calibration position > 420 or end calibration "
			   "position < 420.\n" );
		THROW( EXCEPTION );
	}

    if ( begin >= end )
	{
		print( FATAL, "Begin calibration position not lower than end "
			   "calibration position.\n" );
		THROW( EXCEPTION );
	}

    double step  = get_double( vars_pop( v ), NULL );
    if ( step < 1.0 || step > 50.0 )
	{
        print( FATAL, "Step width smaller than 1 or larger than 50.\n" );
		THROW( EXCEPTION );
	}

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    if ( rename( BBO_OPO_PATH, BBO_OPO_PATH_OLD ) == -1 )
	{
        print( FATAL, "Can't move '%s' to '%s'.\n", BBO_OPO_PATH,
			   BBO_OPO_PATH_OLD );
		THROW( EXCEPTION );
	}

    print( NO_ERROR, "Opo moves to the position entered\n" );
    
    double realBegin = begin;
    if ( begin < opo.CalLambdaBegin )
        realBegin = opo.CalLambdaBegin;

    Var_T * func_ptr = func_get( "opo_goTo", NULL );
    vars_push( FLOAT_VAR, realBegin );
    vars_push( FLOAT_VAR, 10.0 );
    vars_pop( func_call( func_ptr ) );

    print( NO_ERROR, "Opo reached the specified position\n" );
    int g = 0;
    double actualLambda=begin;
    int actualStepPosition = opo_stepsFromLambda( realBegin );

    print( NO_ERROR, "current stepper position: %d\n", actualStepPosition );

    while ( true )
    {
        printf( "Enter additional stepper steps: " );

        char stringE[ 50 ];
        if ( scanf( "%49s", stringE ) != 1 )
			continue;

		if ( ! strncasecmp( stringE, "ok", 2 ) )
		{
			g++;
			FILE * fp = fopen( BBO_OPO_PATH,"a" );
			if ( ! fp )
			{
				print( FATAL, "Can't open '%s' for writing.\n", BBO_OPO_PATH );
				THROW( EXCEPTION );
			}

			fprintf( fp, "%d %f %d\n", g, actualLambda, actualStepPosition );

			fclose( fp );
			print( NO_ERROR, "saved stepper position %d to wavelength "
				   "%f\n", actualStepPosition, actualLambda );
			actualLambda+=step;
			if ( actualLambda > end )
				break;

			print( NO_ERROR, "current calibration wavelength: %f, current "
				   "stepper position: %d\n", actualLambda, actualStepPosition );
		}
		else
		{
			int input = atoi( stringE );
			if ( input == 0 )
				printf( "Incorrect input: just enter an integer or "
                            "enter \"ok\" .");
			else
			{
				if ( actualStepPosition + input < 0 )
				{
					print( NO_ERROR, "Error. Current stepper "
						   "position would be < 0. \n");
					continue;
				}

				actualStepPosition += input;
				opo_stepperSetAbsVarFreq( actualStepPosition, 4000 );
				opo_reachedPosition( actualStepPosition, 0 );
				print( NO_ERROR, "current stepper position: %d\n",
					   actualStepPosition );
			}
		}
    }

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
    expects 1 double variable: opo position in nm as signal

    care about write permissions of BBO_OPO.cal, u.c. u should start fsc2
    with root rights
 *------------------------------------------------------*/

Var_T *
opo_setOffset( Var_T * v )
{
    double nm = get_double( v, NULL );
    if ( nm < opo.CalLambdaBegin || nm > opo.CalLambdaEnd )
    {
        print( FATAL, "Wavelength out of range: %f - %f\n",
               opo.CalLambdaBegin, opo.CalLambdaEnd );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    print( NO_ERROR, "Opo moves to the position entered.\n" );

    Var_T * func_ptr = func_get( "opo_goTo", NULL );
    vars_push( FLOAT_VAR, nm );
    vars_push( FLOAT_VAR, 10.0 );
    vars_pop( func_call( func_ptr ) );

    print( NO_ERROR, "Opo reached the specified position.\n" );

    int startStepPosition = opo_stepsFromLambda( nm );  
    print( NO_ERROR, "current stepper position: %d\n", startStepPosition );

    int actualStepPosition = startStepPosition; 

    while ( true )
    {
        printf( "Enter additional stepper steps:" );

        char stringE[ 50 ];
        if ( scanf( "%49s", stringE ) != 1 )
            continue;

        if ( ! strncasecmp( stringE, "ok", 2 ) )
        {
            int os = actualStepPosition - startStepPosition;

            FILE * fp = fopen( BBO_OPO_PATH, "w" );
            if ( ! fp )
			{
                print( FATAL, "Can't open '%s' for writing.\n", BBO_OPO_PATH );
				THROW( EXCEPTION );
			}

            for ( int g = 1; g <= opo.LengthOfMotorPosArray; ++g )
                fprintf( fp, "%d %f %d\n", g, opo.CalLambda[ g ],
                         opo.CalMotorPos[ g ] + os );

            fclose( fp );

            print( NO_ERROR,"saved offset: %d\n", os );
            break;
        }

        int input = atoi( stringE );

        if ( input == 0 )
        {
            printf( "Incorrect input: just enter an integer or "
                    "enter \"ok\".\n" );
            continue;
        }

        if ( actualStepPosition + input < 0 )
        {
            print( NO_ERROR, "Error. Current stepper position "
                   "would be < 0.\n" );
            continue;
        }

        actualStepPosition += input;
        opo_stepperSetAbsVarFreq( actualStepPosition, 4000 );
        opo_reachedPosition( actualStepPosition, 0 );
        print( NO_ERROR, "current stepper position: %d\n",
               actualStepPosition );
    }

    return vars_push( INT_VAR, 1L );
}


/*------------------------------------------------------*
    expects 1 double variable: opo position in nm as signal
    user input:     "end" -> experiment will finish
            double num -> opo moves to position "num"
 *------------------------------------------------------*/

Var_T *
opo_goToInteractive( Var_T * v )
{
    double nm = get_double( v, NULL );
    if ( nm < opo.CalLambdaBegin || nm > opo.CalLambdaEnd )
    {
        print( FATAL, "Wavelength out of range: %f - %f\n",
               opo.CalLambdaBegin, opo.CalLambdaEnd );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE != EXPERIMENT )
        return vars_push( INT_VAR, 1L );

    print( NO_ERROR, "Opo moves to the position entered.\n" );

    Var_T * func_ptr = func_get( "opo_goTo", NULL );
    vars_push( FLOAT_VAR, nm );
    vars_push( FLOAT_VAR, 10.0 );
    vars_pop( func_call( func_ptr ) );

    print( NO_ERROR, "Opo reached the specified position. The interactive "
           "mode starts.\n" );

    while ( true )
    {
        printf( "Enter wavelength: " );

        char stringE[ 50 ];
        if ( scanf( "%49s", stringE ) != 1 )
            continue;

        if ( ! strncasecmp( stringE, "end", 3 ) )
            break;

        double input = atof( stringE );

        if ( input == 0 )
        {
            printf( "Incorrect input: just enter a float or enter \"end\".\n" );
            continue;
        }

        func_ptr = func_get( "opo_goTo", NULL );
        vars_push( FLOAT_VAR, input );
        vars_push( FLOAT_VAR, 10.0 );
        vars_pop( func_call( func_ptr ) );      
        print( NO_ERROR, "Opo reached the specified position.\n");
    }

    return vars_push( INT_VAR, 1L );
}


/* NON-EDL HELP FUNCTIONS */

/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_sendCommand( const unsigned char * c )
{   
    if ( FSC2_MODE != EXPERIMENT )
        return;

    unsigned char cmd[ 13 ]; 

    cmd[ 0 ] = 0x3c;
    unsigned char checkSum = cmd[ 0 ];
    
    for ( int i = 0; i < 10; i++ )
        checkSum += cmd[ i + 1 ] = c[ i ];

    cmd[ 11 ] = checkSum;
    cmd[ 12 ] = 0x3e;

    ssize_t len = fsc2_serial_write( opo.handle, ( char * ) cmd,
									 sizeof cmd, 100000, UNSET ); 
    if ( len !=sizeof cmd )
    {
        if ( len == 0 )
            opo_failure( "No data could be written to opo" );
        else
            opo_failure( "Can't write complete command to opo" );
    }

	fsc2_usleep( 100000, UNSET );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_setAcceleration( unsigned int StepperStartFrequenz,
                     unsigned int StepperMaxFrequenz,
                     unsigned int StepperAccLength )
{
    unsigned char cmd_str[ 10 ];

    cmd_str[ 0 ] = 0x2;

    cmd_str[ 1 ] =   StepperStartFrequenz         & 0xFF;
    cmd_str[ 2 ] = ( StepperStartFrequenz >>  8 ) & 0xFF;
    cmd_str[ 3 ] = ( StepperStartFrequenz >> 16 ) & 0xFF;

    cmd_str[ 4 ] =   StepperMaxFrequenz         & 0xFF;
    cmd_str[ 5 ] = ( StepperMaxFrequenz >>  8 ) & 0xFF;
    cmd_str[ 6 ] = ( StepperMaxFrequenz >> 16 ) & 0xFF;

    cmd_str[ 7 ] =   StepperAccLength         & 0xFF;
    cmd_str[ 8 ] = ( StepperAccLength >>  8 ) & 0xFF;
    cmd_str[ 9 ] = ( StepperAccLength >> 16 ) & 0xFF;

    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_stepperSetAbsPosition( unsigned int ParAbsolutePosition )
{
    unsigned char cmd_str[ 10 ];

    cmd_str[ 0 ] = 0x7;  // move absolute
    cmd_str[ 1 ] = 1;    // motor#1

    cmd_str[ 2 ] =   ParAbsolutePosition         & 0xFF;
    cmd_str[ 3 ] = ( ParAbsolutePosition >>  8 ) & 0xFF;
    cmd_str[ 4 ] = ( ParAbsolutePosition >> 16 ) & 0xFF;
    cmd_str[ 5 ] = ( ParAbsolutePosition >> 24 ) & 0xFF;

	memset( cmd_str + 6, 0, 4 );

    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_stepperSetAbsVarFreq( unsigned int ParAbsolutePosition,
                          unsigned int GoToSpeed )
{
    unsigned char cmd_str[ 10 ];

    cmd_str[ 0 ] = 0x18; // constant speed absolute move, variable frequency
    cmd_str[ 1 ] = 1;    // motor#1
    cmd_str[ 2 ] = 1;    // start immediately

    cmd_str[ 3 ] =   ParAbsolutePosition         & 0xFF;
    cmd_str[ 4 ] = ( ParAbsolutePosition >>  8 ) & 0xFF;
    cmd_str[ 5 ] = ( ParAbsolutePosition >> 16 ) & 0xFF;
    cmd_str[ 6 ] = ( ParAbsolutePosition >> 24 ) & 0xFF;

    cmd_str[ 7 ] =   GoToSpeed         & 0xFF;
    cmd_str[ 8 ] = ( GoToSpeed >>  8 ) & 0xFF;
    cmd_str[ 9 ] = ( GoToSpeed >> 16 ) & 0xFF;

    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_autoPromptEnable( void )
{
    unsigned char cmd_str[ 10 ];

    cmd_str[ 0 ] = 0xf;
	memset( cmd_str + 1, 1, 9 );   // all the rest is 1

    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_autoPromptDisable( void )
{
    unsigned char cmd_str[ 10 ] = { 0x10  /* all the rest is 0 */};
    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_triggerCount( unsigned int events )
{
    unsigned char cmd_str[ 10 ] = { 0x11 };

    cmd_str[ 1 ] =   events         & 0xFF;
    cmd_str[ 2 ] = ( events >>  8 ) & 0xFF;
    cmd_str[ 3 ] = ( events >> 16 ) & 0xFF;
    cmd_str[ 4 ] = ( events >> 24 ) & 0xFF;

    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_specialWait( void )
{

    unsigned char cmd_str[ 10 ] = { 0x14  /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_promptOnTriggerEnable( void )
{

    unsigned char cmd_str[ 10 ] = { 0x0d  /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_promptOnTriggerDisable( void )
{
    unsigned char cmd_str[ 10 ] = { 0x0e  /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_deceleratingStop( void )
{
    unsigned char cmd_str[ 10 ] = { 0x4  /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );
}



/*------------------------------------------------------*
 *------------------------------------------------------*/

#if 0
static
void
opo_requestPrompt( void )
{
    unsigned char cmd_str[ 10 ] = { 0x17 /* all the rest is 0 */ };
    opo_sendCommand( cmd_str );
}
#endif


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
int
opo_stepsFromLambda( double ParLambda )
{
    int MotorPosDiff;
    double LambdaDiff;
    int i = 0;

    do
    {
        i++;
        LambdaDiff = ParLambda - opo.CalLambda[ i ];
        if ( opo.CalLambda[ i ] == opo.CalLambdaEnd )
        {
            opo.CalLambda[ i + 1 ] =   opo.CalLambda[ i ]
                                     + (   opo.CalLambda[ i ]
                                         - opo.CalLambda[ i - 1 ] );
            opo.CalMotorPos[ i + 1 ] = opo.CalMotorPos[ i ]
                                       + (   opo.CalMotorPos[ i ]
                                           - opo.CalMotorPos[ i - 1 ] );
        }
    } while ( LambdaDiff >=
                         ( opo.CalLambda[ i + 1 ] - opo.CalLambda[ i ] ) / 2 );

    MotorPosDiff = opo.CalMotorPos[ i + 1 ] - opo.CalMotorPos[ i ];

    int sol =   opo.CalMotorPos[ i ]
              + round( (   MotorPosDiff
                         / (   opo.CalLambda[ i + 1 ]
                             - opo.CalLambda[ i ] ) ) * LambdaDiff );
    return sol;
}


/*------------------------------------------------------*
  opo_lambdaFromSteps returns wavelength as Signal
  -opo_lambdaFromSteps pre condition: "steps" is in calibrated range !
 *------------------------------------------------------*/

static
double
opo_lambdaFromSteps( int steps )
{
    double DiffMotPos,
		   LambdaDiff,
		   lambda;

    int i = 0;
    do
	{
        i++;
        DiffMotPos = steps - opo.CalMotorPos[ i ];
        if ( opo.CalMotorPos[ i ] == opo.CalStepEnd )
        {
            opo.CalLambda[ i + 1 ] =   opo.CalLambda[ i ]
                                     + (   opo.CalLambda[ i ]
                                         - opo.CalLambda[ i - 1 ] );
            opo.CalMotorPos[ i + 1 ] =   opo.CalMotorPos[ i ]
                                       + (   opo.CalMotorPos[ i ]
                                           - opo.CalMotorPos[ i - 1 ] );
        }
    } while( DiffMotPos >=
                     ( opo.CalMotorPos[ i + 1 ] - opo.CalMotorPos[ i ] ) / 2 );

    LambdaDiff = opo.CalLambda[ i + 1 ] - opo.CalLambda[ i ];
    lambda =   opo.CalLambda[ i ]
             + (   ( DiffMotPos * LambdaDiff )
                 / ( opo.CalMotorPos[ i + 1 ] - opo.CalMotorPos[ i ] ) );

    return lambda;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

#if 0
static
double
opo_signalFromIdlerIntern( double idler )
{
    return 1.0 / ( 1.0 / 354.767 - 1.0 / idler );
}
#endif


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
double
opo_idlerFromSignalIntern( double opo_signal )
{
    return 1.0 / ( 1.0 / 354.767 - 1.0 / opo_signal );
}


/*------------------------------------------------------*
   b == 0 => calibration or setting offset
   b == 2 => origin search
   b == 1 => all other
 *------------------------------------------------------*/

static
int
opo_reachedPosition( int stepPosition,
                     int b )
{
    int stepPositionErg = -1;

    if ( FSC2_MODE != EXPERIMENT )
        return stepPositionErg;

    time_t t1;
    time( &t1 );

    while ( stepPosition != stepPositionErg )
    {

        const unsigned char * cmd = opo_read( 5000000 );
        if ( cmd[ 0 ] == '[' && cmd[ 13 ] == ']' )
        {
            unsigned int motorStatus= cmd[ 3 ] & 0x1f;
			stepPositionErg = ( cmd[ 6 ] * 256 + cmd[ 5 ] ) * 256 + cmd[ 4 ];

            opo_checkCorrectRange( motorStatus, stepPositionErg, b );

            if ( b == 1 ) // no calibration, offset setting or origin search
            {
                double lambda = opo_lambdaFromSteps( stepPositionErg );      
    
                time_t t2;
                time( &t2 );

                if (    difftime( t2, t1 ) >= 4
                     || stepPosition == stepPositionErg )
                {
                    print( NO_ERROR, "Current position: %f nm\n", lambda );
                    time( &t1 );
                }
            }
        }

		fsc2_usleep( 100000, UNSET );
    }
    
    return stepPositionErg;
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
void
opo_failure( const char * info )
{
    print( FATAL, "%s\n", info );
    THROW( EXCEPTION );
}


/*------------------------------------------------------*
   b == 0 => calibration or offset setting
   b == 2 => origin search
   b == 1 => otherwise
 *------------------------------------------------------*/

static
void
opo_checkCorrectRange( unsigned int motorStatus,
                       int          stepPositionErg,
                       int          b )
{
    if ( b == 0 || b == 1 )  // no origin search
    {
        if ( motorStatus & 4 )
        {
            print( NO_ERROR, "Opo calibration end stop is pressed and the "
                   "Opo moves to the start point from '%s'.\n", BBO_OPO_PATH );

            opo_deceleratingStop( );
            opo_stepperSetAbsVarFreq( opo.CalStepBegin, 4000 );
            opo_reachedPosition( opo.CalStepBegin, 2 );

            print( FATAL, "Opo calibration end stop was pressed and the Opo "
				   "moved up to the start point from '%s'.\n", BBO_OPO_PATH );
			THROW( EXCEPTION );
        }

        if ( motorStatus & 8 )
        {
            print( NO_ERROR,"opo reached the lower limit switch and moves "
                   "up to the start point from '%s'.\n", BBO_OPO_PATH );

            opo_deceleratingStop( );
            opo_stepperSetAbsVarFreq( opo.CalStepBegin, 4000 );
            opo_reachedPosition( opo.CalStepBegin, 2 );

            print( FATAL, "opo reached the lower limit switch and moved "
				   "up to the start point from '%s'.\n", BBO_OPO_PATH );
			THROW( EXCEPTION );
        }

        if ( motorStatus & 16 )
        {
            print( NO_ERROR, "opo reached the upper limit switch and moves "
                   "up to the end point from '%s'.\n", BBO_OPO_PATH );

            opo_deceleratingStop( );
            opo_stepperSetAbsVarFreq( opo.CalStepEnd, 4000 );
            opo_reachedPosition( opo.CalStepEnd, 2 );

            print( FATAL, "opo reached the upper limit switch and moved up "
				   "to the end point from '%s'.\n", BBO_OPO_PATH );
			THROW( EXCEPTION );
        }
    }

    if ( b == 1 )  // no calibration, setting offset or origin search
    {
        

        if ( stepPositionErg < opo.CalStepBegin )
        {
            print( NO_ERROR, "opo is under the calibrated range and moves "
                   "up to the start point from '%s'.\n", BBO_OPO_PATH );

            opo_deceleratingStop( );
            opo_stepperSetAbsVarFreq( opo.CalStepBegin, 4000 );
            opo_reachedPosition( opo.CalStepBegin, 2 );

            print( FATAL, "opo was under the calibrated range and moved up "
				   "to the start point from '%s'.\n", BBO_OPO_PATH );
			THROW( EXCEPTION );
        }

        if ( stepPositionErg > opo.CalStepEnd )
        {
            print( NO_ERROR, "opo is over the calibrated range and moves "
                   "up to the end point from '%s'.\n", BBO_OPO_PATH );

            opo_deceleratingStop( );
            opo_stepperSetAbsVarFreq( opo.CalStepEnd, 4000 );
            opo_reachedPosition( opo.CalStepEnd, 2 );

            print( FATAL, "opo was over the calibrated range and moved up "
				   "to end point from '%s'.\n", BBO_OPO_PATH );
			THROW( EXCEPTION );
        }
    }
}


/*------------------------------------------------------*
 *------------------------------------------------------*/

static
const unsigned char *
opo_read( int us_delay )
{
	static unsigned char reply[ 14 ]; 
	ssize_t len = fsc2_serial_read( opo.handle, ( char * ) reply,
									sizeof reply, NULL, us_delay, SET );

	if ( len != sizeof reply )
	{
		if ( len == 0 )
			opo_failure( "No data could be read from opo" );
		else
			opo_failure( "Incomplete data received from opo" );
	}

	return reply;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
