/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"
#include <sys/socket.h>
#include <sys/un.h>

#include "q_bmwb.conf"

const char device_name[ ]  = DEVICE_NAME;
const char generic_type[ ] = DEVICE_TYPE;


static struct {
	int    fd;
	double freq;
	long   att;
	double signal_phase;
	double bias;
	double lock_phase;
	int    mode;
	double dc_signal;
	double afc_signal;
	int    afc_state;
} q_bmwb;


int q_bmwb_init_hook(       void );
int q_bmwb_exp_hook(        void );
int q_bmwb_end_of_exp_hook( void );
int q_bmwb_exit_hook(       void );


Var_T * mw_bridge_name(             Var_T * v );
Var_T * mw_bridge_run(              Var_T * v );
Var_T * mw_bridge_frequency(        Var_T * v );
Var_T * mw_bridge_attenuation(      Var_T * v );
Var_T * mw_bridge_signal_phase(     Var_T * v );
Var_T * mw_bridge_bias(             Var_T * v );
Var_T * mw_bridge_lock_phase(       Var_T * v );
Var_T * mw_bridge_mode(             Var_T * v );
Var_T * mw_bridge_detector_current( Var_T * v );
Var_T * mw_bridge_afc_signal(       Var_T * v );
Var_T * mw_bridge_afc_state(        Var_T * v );
Var_T * mw_bridge_max_frequency(    Var_T * v );
Var_T * mw_bridge_min_frequency(    Var_T * v );
Var_T * mw_bridge_lock(             Var_T * v );


static int q_bmwb_connect( void );
static ssize_t q_bmwb_write( int          d,
                             const char * buf,
                             ssize_t      len );
static ssize_t q_bmwb_read( int       d,
                            char    * buf,
                            ssize_t   len );
static void q_bmwb_comm_failure( void );


static volatile sig_atomic_t start_finished = 0;
static volatile sig_atomic_t start_failed = 0;


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
q_bmwb_init_hook( void )
{
	q_bmwb.fd           = -1;
	q_bmwb.freq         = 5.0e8 * ( Q_BAND_MIN_FREQ + Q_BAND_MAX_FREQ );
	q_bmwb.att          = 45;
	q_bmwb.signal_phase = 0.5;
	q_bmwb.bias         = 0.5;
	q_bmwb.lock_phase   = 0.5;
	q_bmwb.mode         = 2;
	q_bmwb.dc_signal    = 0.5;
	q_bmwb.afc_signal   = 0.0;
	q_bmwb.afc_state    = 1;

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
q_bmwb_exp_hook( void )
{
	if ( q_bmwb.fd == -1 && q_bmwb_connect( ) )
		THROW( EXCEPTION );

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
q_bmwb_end_of_exp_hook( void )
{
	if ( q_bmwb.fd != -1 )
	{
        shutdown( q_bmwb.fd, SHUT_RDWR );
		close( q_bmwb.fd );
		q_bmwb.fd = -1;
	}

	return 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

int
q_bmwb_exit_hook( void )
{
	if ( q_bmwb.fd != -1 )
	{
        shutdown( q_bmwb.fd, SHUT_RDWR );
		close( q_bmwb.fd );
		q_bmwb.fd = -1;
	}

	return 1;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

Var_T *
mw_bridge_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_run( Var_T * v  UNUSED_ARG )
{
	if ( q_bmwb.fd != -1 )
		return vars_push( INT_VAR, 1L );

	if ( q_bmwb_connect( ) )
		THROW( EXCEPTION );

	shutdown( q_bmwb.fd, SHUT_RDWR );
	close( q_bmwb.fd );
	q_bmwb.fd = -1;

	return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_frequency( Var_T * v )
{
	double freq;
    double orig_freq;
	ssize_t len;
	char buf[ 20 ];


	if ( v == NULL )
	{
		if ( FSC2_MODE != EXPERIMENT )
			return vars_push( INT_VAR, q_bmwb.freq );

		if ( q_bmwb_write( q_bmwb.fd, "FREQ?\n", 6 ) != 6 )
			q_bmwb_comm_failure( );

		if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof buf ) ) == -1
			 || buf[ len - 1 ] != '\n'
			 || ! isdigit( ( unsigned char ) *buf ) )
			q_bmwb_comm_failure( );

		buf[ len -1 ] = '\0';
		return vars_push( FLOAT_VAR,
                          1.0e9 * ( ( Q_BAND_MAX_FREQ - Q_BAND_MIN_FREQ )
                                    * T_atod( buf ) + Q_BAND_MIN_FREQ ) );
	}

    orig_freq = get_double( v, "microwave frequency" );
	freq = 1.0e-9 * orig_freq;

	if ( freq < Q_BAND_MIN_FREQ || freq > Q_BAND_MAX_FREQ )
	{
		print( FATAL, "Invalid frequency argument of %.2f GHz, must be "
			   "between %.2f GHz and %.2f GHz.\n", freq,
			   Q_BAND_MIN_FREQ, Q_BAND_MAX_FREQ );
		THROW( EXCEPTION );
	}

	freq = ( freq - Q_BAND_MIN_FREQ ) / ( Q_BAND_MAX_FREQ - Q_BAND_MIN_FREQ );
	if ( freq < 0.0 )
		freq = 0.0;
	else if ( freq > 1.0 )
		freq = 1.0;

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		q_bmwb.freq = orig_freq;
	else
	{
		sprintf( buf, "FREQ %.3f\n", freq );
		if ( q_bmwb_write( q_bmwb.fd, buf, strlen( buf ) ) == -1 )
			q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
			 || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

	return vars_push( FLOAT_VAR, orig_freq );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_attenuation( Var_T * v )
{
	long att;
	ssize_t len;
	char buf[ 8 ];


	if ( v == NULL )
	{
		if ( FSC2_MODE != EXPERIMENT )
			return vars_push( INT_VAR, q_bmwb.att );

		if ( q_bmwb_write( q_bmwb.fd, "ATT?\n", 5 ) != 5 )
			q_bmwb_comm_failure( );

		if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof buf ) ) == -1
			 || buf[ len - 1 ] != '\n'
			 || ! isdigit( ( unsigned char ) *buf ) )
			q_bmwb_comm_failure( );

		buf[ len -1 ] = '\0';
		return vars_push( INT_VAR, T_atol( buf ) );
	}

	att = get_long( v, "microwave attenuation" );

	if ( att < 0 || att > 60 )
	{
		print( FATAL, "Invalid attenuation argument of %ld dB, must be "
			   "between 0 and 60 dB.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		q_bmwb.att = att;
	else
	{
		sprintf( buf, "ATT %ld\n", att );
		if ( q_bmwb_write( q_bmwb.fd, buf, strlen( buf ) ) == -1 )
			q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
			 || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

	return vars_push( INT_VAR, att );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_signal_phase( Var_T * v )
{
	double phase;
	ssize_t len;
	char buf[ 20 ];


	if ( v == NULL )
	{
		if ( FSC2_MODE != EXPERIMENT )
			return vars_push( INT_VAR, q_bmwb.signal_phase );

		if ( q_bmwb_write( q_bmwb.fd, "SIGPH?\n", 7 ) != 7 )
			q_bmwb_comm_failure( );

		if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof buf ) ) == -1
			 || buf[ len - 1 ] != '\n'
			 || ! isdigit( ( unsigned char ) *buf ) )
			q_bmwb_comm_failure( );

		buf[ len -1 ] = '\0';
		return vars_push( FLOAT_VAR, T_atod( buf ) );
	}

	phase = get_double( v, "signal phase" );

	if ( phase < 0.0 || phase > 1.0 )
	{
		print( FATAL, "Invalid signal phase argument of %.3f, must be "
			   "between 0 and 1.0.\n", phase );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		q_bmwb.signal_phase = phase;
	else
	{
		sprintf( buf, "SIGPH %.6f\n", phase );
		if ( q_bmwb_write( q_bmwb.fd, buf, strlen( buf ) ) == -1 )
			q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
			 || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

	return vars_push( FLOAT_VAR, phase );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_bias( Var_T * v )
{
	double bias;
	ssize_t len;
	char buf[ 20 ];


	if ( v == NULL )
	{
		if ( FSC2_MODE != EXPERIMENT )
			return vars_push( INT_VAR, q_bmwb.bias );

		if ( q_bmwb_write( q_bmwb.fd, "BIAS?\n", 6 ) != 6 )
			q_bmwb_comm_failure( );

		if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof buf ) ) == -1
			 || buf[ len - 1 ] != '\n'
			 || ! isdigit( ( unsigned char ) *buf ) )
			q_bmwb_comm_failure( );

		buf[ len -1 ] = '\0';
		return vars_push( FLOAT_VAR, T_atod( buf ) );
	}

	bias = get_double( v, "bias level" );

	if ( bias < 0.0 || bias > 1.0 )
	{
		print( FATAL, "Invalid bias level argument of %.3f, must be "
			   "between 0 and 1.0.\n", bias );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		q_bmwb.bias = bias;
	else
	{
		sprintf( buf, "BIAS %.6f\n", bias );
		if ( q_bmwb_write( q_bmwb.fd, buf, strlen( buf ) ) == -1 )
			q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
			 || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

	return vars_push( FLOAT_VAR, bias );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_lock_phase( Var_T * v )
{
	double lock_phase;
	ssize_t len;
	char buf[ 20 ];


	if ( v == NULL )
	{
		if ( FSC2_MODE != EXPERIMENT )
			return vars_push( INT_VAR, q_bmwb.lock_phase );

		if ( q_bmwb_write( q_bmwb.fd, "LCKPH?\n", 7 ) != 7 )
			q_bmwb_comm_failure( );

		if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof buf ) ) == -1
			 || buf[ len - 1 ] != '\n'
				|| ! isdigit( ( unsigned char ) *buf ) )
			q_bmwb_comm_failure( );

		buf[ len -1 ] = '\0';
		return vars_push( FLOAT_VAR, T_atod( buf ) );
	}

	lock_phase = get_double( v, "lock phase" );

	if ( lock_phase < 0.0 || lock_phase > 1.0 )
	{
		print( FATAL, "Invalid lock phase argument of %.3f, must be "
			   "between 0 and 1.0.\n", lock_phase );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		q_bmwb.lock_phase = lock_phase;
	else
	{
		sprintf( buf, "LCKPH %.6f\n", lock_phase );
		if ( q_bmwb_write( q_bmwb.fd, buf, strlen( buf ) ) == -1 )
			q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
			 || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

	return vars_push( FLOAT_VAR, lock_phase );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_mode( Var_T * v )
{
	long mode;
	ssize_t len;
	char buf[ 50 ] = "MODE ";
	const char *mstr[ ] = { "STANDBY", "TUNE", "OPERATE" };


	if ( v == NULL )
	{
		if ( FSC2_MODE != EXPERIMENT )
			return vars_push( INT_VAR, q_bmwb.mode );

		if ( q_bmwb_write( q_bmwb.fd, "MODE?\n", 6 ) != 6 )
			q_bmwb_comm_failure( );

		if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof buf ) ) == -1
			 || buf[ len - 1 ] != '\n' )
			q_bmwb_comm_failure( );

		buf[ len -1 ] = '\0';

		for ( mode = 0; mode < 3; mode++ )
			if ( ! strcmp( buf, mstr[ mode ] ) )
				return vars_push( INT_VAR, mode );

		q_bmwb_comm_failure( );
	}

	if ( v->type == STR_VAR )
	{
		for ( mode = 0; mode < 3; mode++ )
			if ( ! strcmp( v->val.sptr, mstr[ mode ] ) )
				break;
	}
	else
		mode = get_strict_long( v, "mode" );

	if ( mode < 0 || mode > 3 )
	{
		print( FATAL, "Invalid mode argument.\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	if ( FSC2_MODE != EXPERIMENT )
		q_bmwb.mode = mode;
	else
	{
		sprintf( buf + 5, "%s\n", mstr[ mode ] );
		if ( q_bmwb_write( q_bmwb.fd, buf, strlen( buf ) ) == -1 )
			q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
			 || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

	return vars_push( INT_VAR, mode );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_detector_current( Var_T * v  UNUSED_ARG )
{
	char buf[ 30 ];
	ssize_t len;


	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, q_bmwb.dc_signal );

	if ( q_bmwb_write( q_bmwb.fd, "DETSIG?\n", 8 ) != 8 )
		q_bmwb_comm_failure( );

	if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof( buf ) ) ) == -1
		 || buf[ len - 1 ] != '\n'
		 || ! isdigit( ( unsigned char ) *buf ) )
		q_bmwb_comm_failure( );

	return vars_push( FLOAT_VAR, T_atod( buf ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_afc_signal( Var_T * v  UNUSED_ARG )
{
	char buf[ 30 ];
	ssize_t len;


	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, q_bmwb.afc_signal );

	if ( q_bmwb_write( q_bmwb.fd, "AFCSIG?\n", 8 ) != 8 )
		q_bmwb_comm_failure( );

	if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof( buf ) ) ) == -1
		 || buf[ len - 1 ] != '\n'
		 || ( *buf != '-' && ! isdigit( ( unsigned char ) *buf ) ) )
		q_bmwb_comm_failure( );

	return vars_push( FLOAT_VAR, T_atod( buf ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_afc_state( Var_T * v  UNUSED_ARG )
{
	char buf[ 5 ];
	ssize_t len;


	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, ( long ) q_bmwb.afc_state );

	if ( q_bmwb_write( q_bmwb.fd, "AFCST?\n", 7 ) != 7 )
		q_bmwb_comm_failure( );

	if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof( buf ) ) ) == -1
		 || buf[ len - 1 ] != '\n'
		 || ( *buf != '0' && *buf != '1' ) )
		q_bmwb_comm_failure( );

	return vars_push( FLOAT_VAR, *buf == '1' ? 1 : 0 );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_max_frequency( Var_T * v  UNUSED_ARG )
{
	char buf[ 30 ];
	ssize_t len;


	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, 1.0e9 * Q_BAND_MAX_FREQ );

	if ( q_bmwb_write( q_bmwb.fd, "MAXFRQ?\n", 8 ) != 8 )
		q_bmwb_comm_failure( );

	if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof( buf ) ) ) == -1
		 || buf[ len - 1 ] != '\n'
		 || ! isdigit( ( unsigned char ) *buf ) )
		q_bmwb_comm_failure( );

	return vars_push( FLOAT_VAR, 1.0e9 * T_atod( buf ) );
}
	


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_min_frequency( Var_T * v  UNUSED_ARG )
{
	char buf[ 30 ];
	ssize_t len;


	if ( FSC2_MODE != EXPERIMENT )
		return vars_push( FLOAT_VAR, 1.0e9 * Q_BAND_MIN_FREQ );

	if ( q_bmwb_write( q_bmwb.fd, "MINFRQ?\n", 8 ) != 8 )
		q_bmwb_comm_failure( );

	if (    ( len = q_bmwb_read( q_bmwb.fd, buf, sizeof( buf ) ) ) == -1
		 || buf[ len - 1 ] != '\n'
		 || ! isdigit( ( unsigned char ) *buf ) )
		q_bmwb_comm_failure( );

	return vars_push( FLOAT_VAR, 1.0e9 * T_atod( buf ) );
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

Var_T *
mw_bridge_lock( Var_T * v )
{
    bool state = get_boolean( v );
    char buf[ 10 ];


    if ( FSC2_MODE == EXPERIMENT )
    {
        sprintf( buf, "LCK %c\n", state ? '1' : '0' );
        if ( q_bmwb_write( q_bmwb.fd, buf, 5 ) != 5 )
            q_bmwb_comm_failure( );

		if (    q_bmwb_read( q_bmwb.fd, buf, 3 ) != 3
             || strncmp( buf, "OK\n", 3 ) )
			q_bmwb_comm_failure( );
	}

    return vars_push( INT_VAR, state ? 1L : 0L );
}


/*--------------------------------------------------------------*
 * Signal handler for SIGUSR1 and SIGUSR2, one of which will be
 * raised when the process controlling the microwave bridge
 * must be started.
 *--------------------------------------------------------------*/

static void
q_bmwb_sig_handler( int signo )
{
	if ( signo == SIGUSR1 )
		start_finished = 1;
	else if ( signo == SIGUSR2 )
		start_failed = 1;
}


/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static int
q_bmwb_connect( void )
{
    int sock_fd;
    struct sockaddr_un serv_addr;
	pid_t pid;
    struct sigaction sact;
    struct sigaction old_s1_act;
    struct sigaction old_s2_act;
	char buf[ 2 ];


    /* Try to get a socket (but first make sure the name isn't too long) */

    if (    sizeof P_tmpdir + 8 > sizeof serv_addr.sun_path
         || ( sock_fd = socket( AF_UNIX, SOCK_STREAM, 0 ) ) == -1 )
	{
		print( FATAL, "Can't connect to microwave bridge.\n" );
        return 1;
	}

    memset( &serv_addr, 0, sizeof serv_addr );
    serv_addr.sun_family = AF_UNIX;
    strcpy( serv_addr.sun_path, P_tmpdir "/bmwb.uds" );

	/* Try to connect to the server */

    if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
                  sizeof serv_addr ) == -1 )
    {
		/* If the error is ENOENT then the socket file doesn't exist and if
		   the error is ECONNREFUSED the server isn't runnng. In this case
		   we have to start the server and try again, otherwise we have to
		   give up. */

		if ( errno != ENOENT && errno != ECONNREFUSED )
		{
			shutdown( sock_fd, SHUT_RDWR );
			close( sock_fd );
			print( FATAL, "Can't connect to microwave bridge.\n" );
			return 1;
		}

		/* The new process for controlling the microwav bridge is supposed to
		   send SIGUSR1 if it's up and running and SIGUSR2 on failure. Set
		   handlers for both signals. */

		sact.sa_handler = q_bmwb_sig_handler;
		sigemptyset( &sact.sa_mask );
		sact.sa_flags = 0;
		if ( sigaction( SIGUSR1, &sact, &old_s1_act ) == -1 )
		{
			print( FATAL, "Can't start program for controlling microwave "
				   "bridge.\n" );
			return 1;
		}

		if ( sigaction( SIGUSR2, &sact, &old_s2_act ) == -1 )
		{
			sigaction( SIGUSR1, &old_s1_act, NULL );
			print( FATAL, "Can't start program for controlling microwave "
				   "bridge.\n" );
			return 1;
		}

		/* Fork the process that deals with the microwave bridge */

		if ( ( pid = fork( ) ) < 0 )
		{
			close( sock_fd );
			close( STDIN_FILENO );
			print( FATAL, "Can't start program handling the microwave "
				   "bridge.\n" );
			return 1;
		}

		if ( pid == 0 )
		{
			char *cmd = NULL;

			close( sock_fd );
			close( STDIN_FILENO );
			cmd = get_string( "%s%sbmwb", bindir, slash( bindir ) );
			execl( cmd, cmd, "-S", NULL );
			kill( getppid( ), SIGUSR2 );
		}

		/* The parent process has to wait for the process dealing with the
		   microwave bridge being either ready or failing. In both cases we'll
		   get a signal (SIGUSR1 or SIGUSR2) handled by a sigbal handler. */

		while ( ! start_finished && ! start_failed )
			fsc2_usleep( 1000, SET );

		sigaction( SIGUSR1, &old_s1_act, NULL );
		sigaction( SIGUSR2, &old_s2_act, NULL );

		if ( start_failed )
		{
			shutdown( sock_fd, SHUT_RDWR );
			close( sock_fd );
			print( FATAL, "Failed to start  program for controlling microwave "
				   "bridge.\n" );
			start_failed = 0;
			return 0;
		}

		start_finished = 0;

		if ( connect( sock_fd, ( struct sockaddr * ) &serv_addr,
					  sizeof serv_addr ) == -1 )
		{
			shutdown( sock_fd, SHUT_RDWR );
			close( sock_fd );
			print( FATAL, "Failed to connect to process for controlling "
				   "microwave bridge.\n" );
			return 1;
		}
	}

	/* We now must be able to read the first reply from the server which is
	   eother 'Q' or 'X', indicating which kind of bridge (X-band of Q-band)
	   the server thinks is controlling, or 'B' if the server is busy or
       'Z" if the server has problems (can't create a thread). */

	if ( q_bmwb_read( sock_fd, buf, 2 ) != 2 || buf[ 1 ] != '\n' )
        *buf = '\0';

    switch ( *buf )
    {
        case 'Q' :
            q_bmwb.fd = sock_fd;
            return 0;

        case 'X' :
            print( FATAL, "Bridge isn't an Q-band bridge.\n" );
            break;

        case 'B' :

            print( FATAL, "Some other process is currently using the "
                   "microwave bridge.\n" );
            break;

        case 'Z' :
            print( FATAL, "Program for controlling microwave bridge doesn't "
                   "work properly.\n" );
            break;

        default :
            print( FATAL, "Process for controlling microwave bridge sends "
                   "invalid data.\n" );
            break;
    }

    shutdown( sock_fd, SHUT_RDWR );
    close( sock_fd );

    return 1;
}


/*-----------------------------------------------------------*
 * Writes as many bytes as was asked for to file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *-----------------------------------------------------------*/

static ssize_t
q_bmwb_write( int          d,
              const char * buf,
              ssize_t      len )
{
    ssize_t n = len,
            ret;


    if ( len == 0 )
        return 0;

    do
    {
        if ( ( ret = write( d, buf, n ) ) < 1 )
        {
            if ( ret == -1 && errno != EINTR )
                return -1;
            continue;
        }
        buf += ret;
    } while ( ( n -= ret ) > 0 );

    return len;
}


/*----------------------------------------------------------------*
 * Reads as many bytes as was asked for from file descriptor,
 * returns the number of bytes on success and -1 otherwise.
 *----------------------------------------------------------------*/

static ssize_t
q_bmwb_read( int       d,
             char    * buf,
             ssize_t   len )
{
    ssize_t n = len,
            ret;


    if ( len == 0 )
        return 0;
	len = 0;

    do
    {
        if ( ( ret = read( d, buf, n ) ) < 1 )
        {
            if ( ret == 0 || errno != EINTR )
                return -1;
            continue;
        }
        buf += ret;
		len += ret;
    } while ( ( n -= ret ) > 0 && buf[ -1 ] != '\n' );

    return len;
}
               

/*--------------------------------------------------------*
 *--------------------------------------------------------*/

static void
q_bmwb_comm_failure( void )
{
	print( FATAL, "Communictation failure.\n" );
	THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
