/*
  $Id$
*/


#include "fsc2.h"
#include "gpib.h"


#define DEVICE_NAME "TDS754A"      /* compare entry in /etc/gpib.conf ! */



/* declaration of exported functions */

int tds754a_init_hook( void );
int tds754a_test_hook( void );
int tds754a_end_of_test_hook( void );
int tds754a_exp_hook( void );
int tds754a_end_of_exp_hook( void );
void tds754a_exit_hook( void );


Var *digitizer_define_window( Var *v );
Var *digitizer_timebase( Var *v );
Var *digitizer_num_averages( Var *v );
Var *digitizer_get_channel_number( Var *v );
Var *digitizer_set_trigger_channel( Var *v );


/* declaration of internally used functions */

static bool tds754a_init( const char *name );
static double tds754a_get_timebase( void );
static bool tds754a_set_timebase( double timebase);
static bool tds754a_get_record_length( long *ret );
static bool tds754a_get_trigger_pos( double *ret );
static long tds754a_get_num_avg( void );
static bool tds754a_set_num_avg( long num_avg );
static int tds754a_get_acq_mode(void);
static bool tds754a_get_cursor_distance( double *cd );
static void tds754a_gpib_failure( void );
static const char *tds754a_ptime( double time );
static void delete_windows( void );
static void tds754a_do_pre_exp_checks( void );
static bool tds754a_clear_SESR( void );
static void tds754a_finished( void );



typedef struct _W {
	long num;
	double start;
	double width;
	bool is_width;
	struct _W * next;
} WINDOW;


typedef struct
{
	int device;

	double timebase;
	bool is_timebase;

	double num_avg;
	bool is_num_avg;

	WINDOW *w;
	int num_windows;

	long rec_len;
	double trig_pos;

} TDS754A;


static TDS754A tds754a;

enum {
	SAMPLE,
	AVERAGE
};


#define TDS_POINTS_PER_DIV 50

const char *Channel_Names[ ] = { "CH1", "CH2", "CH3", "CH4", "AUX",
								 "MATH1", "MATH2", "MATH3", "REF1",
								 "REF2", "REF3", "REF4" };
#define MAX_CHANNELS  11         /* number of channel names */

#define TDS754A_CH1    0
#define TDS754A_CH2    1
#define TDS754A_CH3    2
#define TDS754A_CH4    3
#define TDS754A_AUX    4
#define TDS754A_MATH1  5
#define TDS754A_MATH2  6
#define TDS754A_MATH3  7
#define TDS754A_REF1   8
#define TDS754A_REF2   9
#define TDS754A_REF3  10
#define TDS754A_REF4  11



/*******************************************/
/*   We start with the hook functions...   */
/*******************************************/

/*------------------------------------*/
/* Init hook function for the module. */
/*------------------------------------*/

int tds754a_init_hook( void )
{
	/* Set global variable to indicate that GPIB bus is needed */

	need_GPIB = SET;

	/* Initialize some variables in the digitizers structure */

	tds754a.w = NULL;
	tds754a.is_timebase = UNSET;
	tds754a.is_num_avg = UNSET;
	tds754a.num_windows = 0;

	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int tds754a_test_hook( void )
{
	return 1;
}


/*------------------------------------*/
/* Test hook function for the module. */
/*------------------------------------*/

int tds754a_end_of_test_hook( void )
{
	return 1;
}


/*--------------------------------------------------*/
/* Start of experiment hook function for the module */
/*--------------------------------------------------*/

int tds754a_exp_hook( void )
{
   	WINDOW *w;
	double width;


	if ( ! tds754a_init( DEVICE_NAME ) )
	{
		eprint( FATAL, "TDS754A: Initialization of device failed." );
		THROW( EXCEPTION );
	}

	tds754a_do_pre_exp_checks( );

	/* Test if for all window the width is set */

	for ( w = tds754a.w; w != NULL; w = w->next )
		if ( ! w->is_width )
			break;

	/* If not get the distance of the cursors on the digitizers screen
	   and use it as the default width */

	if ( w != NULL )
	{
		tds754a_get_cursor_distance( &width );

			for ( w = tds754a.w; w != NULL; w = w->next )
				if ( ! w->is_width )
				{
					w->width = width;
					w->is_width = SET;
				}
	}

	return 1;
}


/*------------------------------------------------*/
/* End of experiment hook function for the module */
/*------------------------------------------------*/

int tds754a_end_of_exp_hook( void )
{
#ifndef MAX_DEBUG
	tds754a_finished( );
	gpib_local( tds754a.device );
#endif

	return 1;
}


/*------------------------------------------*/
/* For final work before module is unloaded */
/*------------------------------------------*/


void tds754a_exit_hook( void )
{
	delete_windows( );
}



/*------------------------------------------*/
/*------------------------------------------*/

Var *digitizer_define_window( Var *v )
{
	long win_num;
	double win_start, win_width;
	bool is_win_width = UNSET;
	WINDOW *w;


	if ( v == NULL || v->next == NULL )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Missing parameter in call of "
				"function `digitizer_define_window', need at least two.",
				Fname, Lc );
		THROW( EXCEPTION );
	}

	/* Get the window number */

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		win_num = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: TDS754A: Floating point number used as window "
				"number.", Fname, Lc );
		win_num = lround( v->val.dval );
	}

	v = vars_pop( v );

	/* Test if the window number non-negative and no window with this number
	   already exists */

	if ( win_num < 0 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid negative window number: "
				"%ld.", Fname, Lc, win_num );
		THROW( EXCEPTION );
	}

	for ( w = tds754a.w; w != NULL; w = w->next )
		if ( ( long ) w->num == win_num )
		{
			eprint( FATAL, "%s:%ld: Window %ld has already been defined.",
					Fname, Lc, win_num );
			THROW( EXCEPTION );
		}

	/* Now get the start point of the window */

	vars_check( v, INT_VAR | FLOAT_VAR );
	win_start = VALUE( v );
	v = vars_pop( v );

	/* Finally, if there's another parameter take it to be the window
	   width */

	if ( v != NULL )
	{
			vars_check( v, INT_VAR | FLOAT_VAR );
			win_width = VALUE( v );
			is_win_width = SET;

			v = vars_pop( v );

			if ( v != NULL )
			{
				eprint( WARN, "%s:%ld: Superfluous arguments in call of "
						"function `digitizer_define_window'.", Fname, Lc );

				while ( ( v = vars_pop( v ) ) != NULL )
					;
			}
	}

	/* Create a new window structure and append it to the list of windows */

	if ( tds754a.w == NULL )
		tds754a.w = w = T_malloc( sizeof( WINDOW ) );
	else
	{
		w = tds754a.w;
		while ( w->next != NULL )
			w = w->next;
		w->next = T_malloc( sizeof( WINDOW ) );
		w = w->next;
	}

	w->next = NULL;
	w->num = win_num;
	w->start = win_start;

	if ( is_win_width )
	{
		w->width = win_width;
		w->is_width = SET;
	}
	else
		w->is_width = UNSET;

	tds754a.num_windows++;

	return vars_push( INT_VAR, 1 );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_timebase( Var *v )
{
	double timebase;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_timebase )
				return vars_push( FLOAT_VAR, tds754a.timebase );
			else
				return vars_push( FLOAT_VAR, 0.1 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: TDS754A: Function `digitizer_timebase' "
					"with no argument can only be used in the EXPERIMENT "
					"section.", Fname, Lc );
			THROW( EXCEPTION );
		}

		tds754a.timebase = tds754a_get_timebase( );
		tds754a.is_timebase = SET;
		return vars_push( FLOAT_VAR, tds754a.timebase );
	}

	if ( I_am == CHILD || TEST_RUN )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Digitizer time base can only be set "
				"before the EXPERIMENT section starts.", Fname, Lc );
		THROW( EXCEPTION );
	}

	if ( tds754a.is_timebase )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Digitizer time base has already been "
				"set.", Fname, Lc );
		THROW( EXCEPTION );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	timebase = VALUE( v );
	vars_pop( v );

	if ( timebase <= 0 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid zero or negative time base: "
				"%s.",Fname, Lc, tds754a_ptime( timebase ) );
		THROW( EXCEPTION );
	}

	tds754a.timebase = timebase;
	tds754a.is_timebase = SET;

	return vars_push( FLOAT_VAR, tds754a.timebase );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

Var *digitizer_num_averages( Var *v )
{
	double num_avg;
	

	if ( v == NULL )
	{
		if ( TEST_RUN )
		{
			if ( tds754a.is_num_avg )
				return vars_push( INT_VAR, tds754a.num_avg );
			else
				return vars_push( INT_VAR, 16 );
		}
		else if ( I_am == PARENT )
		{
			eprint( FATAL, "%s:%ld: TDS754A: Function "
					"`digitizer_num_averages' with no argument can only be "
					"used in the EXPERIMENT section.", Fname, Lc );
			THROW( EXCEPTION );
		}

		tds754a.num_avg = tds754a_get_num_avg( );
		tds754a.is_num_avg = SET;
		return vars_push( INT_VAR, tds754a.num_avg );
	}

	vars_check( v, INT_VAR | FLOAT_VAR );
	if ( v->type == INT_VAR )
		num_avg = v->val.lval;
	else
	{
		eprint( WARN, "%s:%ld: TDS754A: Floating point number used as number "
				"of averages.", Fname, Lc );
		num_avg = lround( v->val.dval );
	}
	vars_pop( v );

	if ( num_avg < 0 )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid negative number of averages: "
				"%ld.", Fname, Lc, num_avg );
		THROW( EXCEPTION );
	}

	tds754a.num_avg = num_avg;
	if ( I_am == CHILD )
		tds754a_set_num_avg( num_avg );
	if ( ! TEST_RUN )                 // store value if in PREPARATIONS section
		tds754a.is_num_avg = SET;

	return vars_push( INT_VAR, tds754a.num_avg );
}


/*-------------------------------------------------------------------*/
/*-------------------------------------------------------------------*/

Var *digitizer_get_channel_number( Var *v )
{
	int i;


	vars_check( v, STR_VAR );

	for ( i = 0; i < MAX_CHANNELS; i++ )
		if ( ! strcmp( v->val.sptr, Channel_Names[ i ] ) )
			return vars_push( INT_VAR, i );

	return NULL;
}



/*-------------------------------------------------------------------*/
/* digitizer_set_trigger_channel() sets the channel that is used for */
/* triggering.                                                       */
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
/*-------------------------------------------------------------------*/


Var *digitizer_set_trigger_channel( Var *v )
{
    char cmd[ 40 ] = "TRIG:MAI:EDGE:SOU ";


	vars_check( v, INT_VAR );

	if ( v->val.lval < 0 || v->val.lval >= MAX_CHANNELS )
	{
		eprint( FATAL, "%s:%ld: TDS754A: Invalid channel name.\n",
				Fname, Lc );
		THROW( EXCEPTION );
	}

    switch ( v->val.lval )
    {
        case TDS754A_CH1 : case TDS754A_CH2 : case TDS754A_CH3 :
		case TDS754A_CH4 : case TDS754A_AUX :
            strcat( cmd, Channel_Names[ v->val.lval ] );
            break;

		default :
			eprint( FATAL, "%s:%ld: TDS754A: Channel %s can't be used as "
					"trigger channel.\n", Fname, Lc,
					Channel_Names[ v->val.lval ] );
			THROW( EXCEPTION );
    }

    if ( gpib_write( tds754a.device, cmd, strlen( cmd ) ) == FAILURE )
		tds754a_gpib_failure( );		

	return vars_push( INT_VAR, 1 );
}

/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static bool tds754a_init( const char *name )
{
	if ( gpib_init_device( name, &tds754a.device ) == FAILURE )
        return FAIL;

	/* Set digitzer into local lockout state */

	gpib_lock( tds754a.device );

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
		
    /* Set unit for cursor setting commands to seconds, cursor types to VBAR
       and mode to independent */

    if ( gpib_write( tds754a.device, "CURS:FUNC VBA", 13 )       == FAILURE ||
         gpib_write( tds754a.device, "CURS:MOD IND", 12 )        == FAILURE ||
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
       5. restrict set area for measurements to the area between the cursors
    */

    if ( gpib_write( tds754a.device, "ACQ:STOPA SEQ", 13 ) == FAILURE ||
         gpib_write( tds754a.device, "DESE 1", 6 )         == FAILURE ||
         gpib_write( tds754a.device, "*ESE 1", 6 )         == FAILURE ||
         gpib_write( tds754a.device, "*SRE 32", 7 )        == FAILURE ||
         gpib_write( tds754a.device, "DAT SNA", 7 )        == FAILURE ||
         gpib_write( tds754a.device, "MEASU:GAT ON", 12 ) == FAILURE )
    {
        gpib_local( tds754a.device );
        return FAIL;
    }

	return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static double tds754a_get_timebase( void )
{
	char reply[ 30 ];
	long length;


	if ( gpib_write( tds754a.device, "HOR:MAI:SCA?", 12 ) == FAILURE ||
		 gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

	reply[length - 1] = '\0';
	return atof( reply );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static bool tds754a_set_timebase( double timebase )
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

static bool tds754a_get_record_length( long *ret )
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

static bool tds754a_get_trigger_pos( double *ret )
{
    char reply[ 30 ];
    long length = 30;


    if ( gpib_write( tds754a.device, "HOR:TRIG:POS?", 13 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
        return FAIL;

    reply[ length - 1 ] = '\0';
    *ret = 0.01 * atof( reply );
    return OK;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

static long tds754a_get_num_avg( void )
{
	char reply[30];
	long length = 30;


	if ( tds754a_get_acq_mode() == AVERAGE )
	{
		if ( gpib_write( tds754a.device,"ACQ:NUMAV?", 10 ) == FAILURE ||
			 gpib_read( tds754a.device, reply, &length) == FAILURE )
			tds754a_gpib_failure( );

		reply[length - 1] = '\0';
		return atol(reply);
	}
	else                            	/* digitizer is in sample mode */
		return 1;
}


/*-----------------------------------------*/
/* Function returns the number of averages */
/*-----------------------------------------*/

static bool tds754a_set_num_avg( long num_avg )
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

static int tds754a_get_acq_mode(void)
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

static bool tds754a_get_cursor_distance( double *cd )
{
    char reply[ 30 ];
    long length;


    if ( gpib_write( tds754a.device, "CURS:VBA:POSITION2?", 19 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cd = atof( reply );

    length = 30;
    if ( gpib_write( tds754a.device, "CURS:VBA:POSITION1?", 19 ) == FAILURE ||
         gpib_read( tds754a.device, reply, &length ) == FAILURE )
		tds754a_gpib_failure( );

    reply[ length - 1 ] = '\0';
    *cd -= atof( reply );

    return OK;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void tds754a_gpib_failure( void )
{
	eprint( FATAL, "TDS754A: Communication with device failed." );
	THROW( EXCEPTION );
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static const char *tds754a_ptime( double time )
{
	static char buffer[ 128 ];

	if ( fabs( time ) >= 1.0 )
		sprintf( buffer, "%g s", time );
	else if ( fabs( time ) >= 1.e-3 )
		sprintf( buffer, "%g ms", 1.e3 * time );
	else if ( fabs( time ) >= 1.e-6 )
		sprintf( buffer, "%g us", 1.e6 * time );
	else
		sprintf( buffer, "%g ns", 1.e9 * time );

	return buffer;
}


/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

static void delete_windows( void )
{
	WINDOW *w;

	while ( tds754a.w != NULL )
	{
		w = tds754a.w;
		tds754a.w = w->next;
		T_free( w );
	}
}


static void tds754a_do_pre_exp_checks( void )
{
	WINDOW *w;
    double width, window, dcd, dtb, fac;
    long tb, cd;


	/* Test if for all window the width is set */

	for ( w = tds754a.w; w != NULL; w = w->next )
		if ( ! w->is_width )
			break;

	/* If not get the distance of the cursors on the digitizers screen and
	   use it as the default width. */

	if ( w != NULL )
	{
		tds754a_get_cursor_distance( &width );

		if ( width == 0.0 )
		{
			eprint( FATAL, "TDS754A: Can't determine a reasonable value for "
					"the missing window widths." );
			THROW( EXCEPTION );
		}

		for ( w = tds754a.w; w != NULL; w = w->next )
		{
			if ( ! w->is_width )
			{
				w->width = width;
				w->is_width = SET;
			}
		}
	}

	/* It's not possible to set arbitrary cursor distances - they've got to be
	   multiples of the smallest time resolution of the digitizer, which is
	   1 / TDS_POINTS_PER_DIV of the time base. In the following it is tested
	   if the requested cursor distance fit this requirement and if not the
	   values are readjusted. */

	for ( w = tds754a.w; w != NULL; w = w->next )
	{
		dcd = w->width;
		dtb = tds754a.timebase;
		fac = 1.0;
		while ( fabs( dcd ) < 1.0 )
		{
			dcd *= 1000.0;
			dtb *= 1000.0;
			fac *= 0.001;
		}
		cd = lround( TDS_POINTS_PER_DIV * dcd );
		tb = lround( dtb );

		if ( labs( cd ) < tb )
		{
			w->width = tds754a.timebase / TDS_POINTS_PER_DIV;
			eprint( SEVERE, "TDS754A: Width of window %ld has to be "
					"readjusted to %s.", w->num, tds754a_ptime( w->width  ) );
		}
		else if ( cd % tb )
		{
			cd = ( cd / tb ) * tb;
			dcd = cd * fac / TDS_POINTS_PER_DIV;
			eprint( SEVERE, "TDS754A; Width of window %ld has to be "
					"readjusted to %s.", w->num, tds754a_ptime( dcd ) );
			w->width = dcd;
		}
	}

	/* Next check: Test if the windows fit into the measurement window */

    window = tds754a.timebase * tds754a.rec_len / TDS_POINTS_PER_DIV;

    for ( w = tds754a.w; w != NULL; w = w->next )
    {
        if ( w->start > ( 1.0 - tds754a.trig_pos ) * window ||
             w->start + w->width > ( 1.0 - tds754a.trig_pos ) * window ||
             w->start < - tds754a.trig_pos * window ||
             w->start + w->width < - tds754a.trig_pos * window )
        {
			eprint( FATAL, "TDS754A: Window %ld doesn't fit into current "
					"digitizer time range.", w->num );
			THROW( EXCEPTION );
		}
    }
}

/*-------------------------------------------------------------------*/
/* tds754a_clear_SESR() reads the the standard event status register */
/* and thereby clears it - if this isn't done no SRQs are flagged !  */
/*-------------------------------------------------------------------*/

static bool tds754a_clear_SESR( void )
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

static void tds754a_finished( void )
{
    tds754a_clear_SESR( );
    gpib_write( tds754a.device, "ACQ:STATE STOP", 14 );

    gpib_write( tds754a.device, "*SRE 0", 5 );
    gpib_write( tds754a.device, "ACQ:STOPA RUNST", 15 );
    gpib_write( tds754a.device, "ACQ:STATE RUN", 13 );
    gpib_write( tds754a.device, "LOC NON", 7 ); 
}
