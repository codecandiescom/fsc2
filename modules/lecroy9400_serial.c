/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#include "lecroy9400_s.h"


static bool is_acquiring = UNSET;
static bool lecroy9400_talk( const char *cmd, char *reply, long *length );
static bool lecroy9400_serial_init( void );
static bool lecroy9400_write( const char *buffer, long length );
static bool lecroy9400_read( char *buffer, long *length );
static void lecroy9400_local( void );


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_init( void )
{
    char buffer[ 100 ];
    long len = sizeof buffer;
    int i;


    is_acquiring = UNSET;

    if ( ! lecroy9400_serial_init( ) )
        return FAIL;

    /* Set digitizer to short form of replies, make it only send a line feed
       at the end of replies, switch off debugging, transmit data in one block
       without an END block marker (#I) and set the data format to ASCII (we
       can't set it to binary when talking to the device over RS232) with
       16-bit data. Then ask it for the status byte 1 to test if the device
       reacts. */

    if (    lecroy9400_write(
                             "CHDR,OFF;CTRL,LF;CHLP,OFF;CBLS,-1;CFMT,L,WORD\n",
                             46 ) == FAIL
         || lecroy9400_write( "STB,1\n", 6 ) == FAIL
         || lecroy9400_read( buffer, &len ) == FAIL )
    {
        lecroy9400_finished( );
        return FAIL;
    }

    lecroy9400.is_reacting = SET;
    lecroy9400.data_size = 1;

    /* Set or get the time base, if necessary change it to a valid value */

    if ( lecroy9400.is_timebase )
        lecroy9400_set_timebase( lecroy9400.timebase );
    else
    {
        lecroy9400.timebase = lecroy9400_get_timebase( );
        if ( lecroy9400.timebase < tb[ 0 ] )
        {
            lecroy9400_set_timebase( lecroy9400.timebase = tb[ 0 ] );
            print( SEVERE, "Timebase too fast, changing to %s.\n",
                   lecroy9400_ptime( lecroy9400.timebase ) );
        }
        else if ( lecroy9400.timebase > tb[ NUM_ELEMS( tb ) - 1 ] )
        {
            lecroy9400_set_timebase( lecroy9400.timebase
                                     = tb[ NUM_ELEMS( tb ) - 1 ] );
            print( SEVERE, "Timebase too slow, changing to %s.\n",
                   lecroy9400_ptime( lecroy9400.timebase ) );
        }

        lecroy9400.tb_index = lecroy9400_get_tb_index( lecroy9400.timebase );
    }

    /* After we're now reasonable sure that the time constant is 50 ns/div
       or longer we can switch off interleaved mode */

    if ( lecroy9400_write( "IL,OFF\n", 7 ) == FAIL )
        return FAIL;

    /* Now get the waveform descriptors for all channels */

    for ( i = LECROY9400_CH1; i <= LECROY9400_FUNC_F; i++ )
        lecroy9400_get_desc( i );

    /* Set up averaging for function channels */

    for ( i = LECROY9400_FUNC_E; i <= LECROY9400_FUNC_F; i++ )
        if ( lecroy9400.is_num_avg[ i ] )
            lecroy9400_set_up_averaging( i, lecroy9400.source_ch[ i ],
                                         lecroy9400.num_avg[ i ],
                                         lecroy9400.is_reject[ i ],
                                         lecroy9400.rec_len[ i ] );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_timebase( void )
{
    char reply[ 30 ];
    long length = sizeof reply;


    lecroy9400_talk( "TD,?\n", reply, &length );
    reply[ length - 2 ] = '\0';
    return T_atod( reply );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_timebase( double timebase )
{
    char cmd[ 40 ] = "TD,";


    gcvt( timebase, 6, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_trigger_source( void )
{
    char reply[ 30 ];
    long length = sizeof reply;
    int src;


    lecroy9400_talk( "TRD,?\n", reply, &length );

    if ( reply[ 0 ] == 'C' )
        src = reply[ 1 ] == '1' ? LECROY9400_CH1 : LECROY9400_CH2;
    else if ( reply[ 0 ] == 'L' )
        src = LECROY9400_LIN;
    else if ( reply[ 0 ] == 'E' )
        src = reply[ 1 ] == 'X' ? LECROY9400_EXT : LECROY9400_EXT10;
    else
        fsc2_impossible( );

    return src;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_source( int channel )
{
    char cmd[ 40 ] = "TRS,";


    fsc2_assert(    channel == LECROY9400_CH1 || channel == LECROY9400_CH2
                 || ( channel >= LECROY9400_LIN
                      && channel <= LECROY9400_EXT10 ) );

    if ( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 )
        sprintf( cmd + 4, "C%1d\n", channel + 1 );
    else if ( channel == LECROY9400_LIN )
        strcat( cmd, "LI\n" );
    else if ( channel == LECROY9400_EXT )
        strcat( cmd, "EX\n" );
    else
        strcat( cmd, "E/10\n" );

    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_trigger_level( void )
{
    char reply[ 30 ];
    long length = sizeof reply;


    lecroy9400_talk( "TRL,?\n", reply, &length );
    reply[ length - 2 ] = '\0';
    return T_atod( reply );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_level( double level )
{
    char cmd[ 40 ] = "TRL,";


    gcvt( level, 6, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_sens( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = sizeof reply;


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dVD,?\n", channel + 1 );
    lecroy9400_talk( cmd, reply, &length );
    reply[ length - 2 ] = '\0';
    lecroy9400.sens[ channel ] = T_atod( reply );

    return lecroy9400.sens[ channel ];
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_sens( int channel, double sens )
{
    char cmd[ 40 ];


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dVD,", channel + 1 );
    gcvt( sens, 8, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );

    lecroy9400_get_desc( channel );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_offset( int channel )
{
    char cmd[ 20 ];
    char reply[ 30 ];
    long length = sizeof reply;


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dOF,?\n", channel + 1 );
    lecroy9400_talk( cmd, reply, &length );
    reply[ length - 2 ] = '\0';
    lecroy9400.sens[ channel ] = T_atod( reply );

    return lecroy9400.sens[ channel ];
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_offset( int channel, double offset )
{
    char cmd[ 40 ];


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dOF,", channel + 1 );
    gcvt( offset, 8, cmd + strlen( cmd ) );
    strcat( cmd, "\n" );
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );

    lecroy9400_get_desc( channel );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

int
lecroy9400_get_coupling( int channel )
{
    char cmd[ 20 ];
    int type;
    char reply[ 100 ];
    long length = sizeof reply;


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );

    sprintf( cmd, "C%1dCP,?\n", channel + 1 );
    lecroy9400_talk( cmd, reply, &length );
    reply[ length - 2 ] = '\0';

    if ( reply[ 0 ] == 'A' )
        type = AC_1_MOHM;
    else if ( reply[ 1 ] == '1' )
        type = DC_1_MOHM;
    else
        type = DC_50_OHM;

    return type;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_coupling( int channel, int type )
{
    char cmd[ 30 ];
    char const *cpl[ ] = { "A1M", "D1M", "D50" };


    fsc2_assert( channel == LECROY9400_CH1 || channel == LECROY9400_CH2 );
    fsc2_assert( type >= AC_1_MOHM && type <= DC_50_OHM );


    sprintf( cmd, "C%1dCP,%s\n", channel + 1, cpl[ type ] );
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );

    lecroy9400_get_desc( channel );

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_is_displayed( int channel )
{
    char reply[ 30 ];
    long length = sizeof reply;
    char const *cmds[ ] = { "TRC1,?\n", "TRC2,?\n", "TRMC,?\n", "TRMD,?\n",
                            "TRFE,?\n", "TRFF,?\n" };


    fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

    lecroy9400_talk( cmds[ channel - LECROY9400_CH1 ], reply, &length );
    reply[ length - 2 ] = '\0';
    return strcmp( reply, "ON" ) ? UNSET : SET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_display( int channel, int on_off )
{
    char cmds[ ][ 9 ] = { "TRC1,", "TRC2,", "TRMC,", "TRMD,",
                          "TRFE,", "TRFF," };


    fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

    if ( on_off && lecroy9400.num_used_channels >= MAX_USED_CHANNELS )
    {
        print( FATAL, "More than %d channels are needed to run the "
               "experiment.\n", MAX_USED_CHANNELS );
        THROW( EXCEPTION );
    }

    strcat( cmds[ channel - LECROY9400_CH1 ], on_off ? "ON\n" : "OFF\n" );

    if ( lecroy9400_write( cmds[ channel - LECROY9400_CH1 ],
                         strlen( cmds[ channel - LECROY9400_CH1 ] ) ) == FAIL )
        lecroy9400_comm_failure( );

    if ( on_off )
    {
        lecroy9400.num_used_channels++;
        lecroy9400.channels_in_use[ channel ] = SET;
    }
    else
    {
        lecroy9400.num_used_channels--;
        lecroy9400.channels_in_use[ channel ] = UNSET;
    }

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

long
lecroy9400_get_num_avg( int channel )
{
    int i;
    long num_avg;


    fsc2_assert(    channel == LECROY9400_FUNC_E
                 || channel == LECROY9400_FUNC_F );

    /* Check that the channel is in averaging mode (in which case the byte
       at offset 34 of the waveform description is 0), if not return -1 */

    if ( ( int ) lecroy9400.wv_desc[ channel ][ 34 ] != 99 )
        return -1;

    /* 32 bit word (MSB first) at offset 70 of the waveform description is
       the number of averages */

    for ( num_avg = 0, i = 70; i < 74; i++ )
        num_avg = num_avg * 256 + ( long ) lecroy9400.wv_desc[ channel ][ i ];

    return num_avg;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_get_desc( int channel )
{
    long len = MAX_DESC_LEN;
    char const *cmds[ ] = { "RD,C1.DE\n", "RD,C2.DE\n",
                            "RD,MC.DE\n", "RD,MD.DE\n",
                            "RD,FE.DE\n", "RD,FF.DE\n" };


    fsc2_assert( channel >= LECROY9400_CH1 && channel <= LECROY9400_FUNC_F );

    lecroy9400_talk( cmds[ channel ],
                     ( char * ) lecroy9400.wv_desc[ channel ], &len );
    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

double
lecroy9400_get_trigger_pos( void )
{
    /***** Still needs to be implemented *****/

    return 0;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

bool
lecroy9400_set_trigger_pos( double position )
{
    UNUSED_ARGUMENT( position );

    /***** Still needs to be implemented *****/

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_set_up_averaging( long channel,
                             long source,
                             long num_avg,
                             bool reject,
                             long rec_len )
{
    char cmd[ 100 ];
    size_t i;


    fsc2_assert(    channel >= LECROY9400_FUNC_E
                 && channel <= LECROY9400_FUNC_F );
    fsc2_assert( source >= LECROY9400_CH1 && source <= LECROY9400_CH2 );

    lecroy9400.channels_in_use[ channel ] = SET;
    lecroy9400.channels_in_use[ source ] = SET;
    lecroy9400.source_ch[ channel ] = source;
    lecroy9400.num_avg[ channel ] = num_avg;
    lecroy9400.is_num_avg[ channel ] = SET;
    lecroy9400.rec_len[ channel ] = rec_len;
    lecroy9400.is_reject[ channel ] = reject;

    if ( FSC2_MODE != EXPERIMENT )
        return;

    /* If the record length hasn't been set use the number of points on the
       screen (which depends on the time base). Otherwise check that the
       number we got isn't larger than the number of points on the screen
       and, if necessary reduce it. */

    if ( rec_len == UNDEFINED_REC_LEN )
        rec_len = lecroy9400.rec_len[ channel ] = ml[ lecroy9400.tb_index ];
    else
    {
        if ( rec_len > ml[ lecroy9400.tb_index ] )
        {
            print( SEVERE, "Record length of %ld too large, using %ld "
                   "instead\n", rec_len, ml[ lecroy9400.tb_index ] );
            rec_len = lecroy9400.rec_len[ channel ] =
                                                     ml[ lecroy9400.tb_index ];
        }
        else
            lecroy9400.rec_len[ channel ] = rec_len;

    }

    /* We need to send the digitizer the number of points it got to average
       over. This number isn't identical to the number of points on the
       screen, so we pick one of the list of allowed values that's at least
       as large as the number of points we're going to fetch. */

    for ( i = 0; i < NUM_ELEMS( cl ) && rec_len > cl[ i ]; i++ )
        /* empty */ ;
    rec_len = cl[ i ];

    snprintf( cmd, 100, "SEL,%s;RDF,AVG,SUMMED,%ld,%s,%ld,%s,0\n",
              channel == LECROY9400_FUNC_E ? "FE" : "FF", rec_len,
              source == LECROY9400_CH1 ? "C1" : "C2", num_avg,
              reject ? "ON" : "OFF" );
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );
}



/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_finished( void )
{
    lecroy9400_local( );
    fsc2_serial_close( lecroy9400.device );
    is_acquiring = UNSET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_start_acquisition( void )
{
    if ( lecroy9400.channels_in_use[ LECROY9400_FUNC_E ] )
        if ( lecroy9400_write( "SEL,FE;ARST\n", 12 ) == FAIL )
            lecroy9400_comm_failure( );

    if ( lecroy9400.channels_in_use[ LECROY9400_FUNC_F ] )
        if ( lecroy9400_write( "SEL,FF;ARST\n", 12 ) == FAIL )
            lecroy9400_comm_failure( );

    is_acquiring = SET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_get_curve( int        ch,
                      Window_T * w,
                      double **  array,
                      long *     length,
                      bool       use_cursor )
{
    unsigned char *data;
    unsigned char *dp;
    char cmd[ 100 ];
    long len;
    long i;
    double gain_fac, vgain_fac, offset_shift;


    UNUSED_ARGUMENT( w );
    UNUSED_ARGUMENT( use_cursor );

    if ( ch >= LECROY9400_CH1 && ch <= LECROY9400_CH2 )
    {
        print( FATAL, "Getting normal, non-averaged channels is not "
               "yet implemented.\n" );
        THROW( EXCEPTION );
    }

    if ( ch >= LECROY9400_MEM_C && ch <= LECROY9400_MEM_D )
    {
        print( FATAL, "Getting memory channels is not yet implemented.\n" );
        THROW( EXCEPTION );
    }

    fsc2_assert( ch == LECROY9400_FUNC_E || ch == LECROY9400_FUNC_F );

    if ( ! lecroy9400.is_num_avg[ ch ] )
    {
        print( FATAL, "Averaging has not been initialized for channel %s.\n",
               LECROY9400_Channel_Names[ ch ] );
        THROW( EXCEPTION );
    }

    if ( ! is_acquiring )
    {
        print( FATAL, "No acquisition has been started.\n" );
        THROW( EXCEPTION );
    }

    /* Poll the device until averaging is finished, i.e. get the channel
       description and check if there are as many averages as we need */

    while ( 1 )
    {
        long cur_avg;
        long j;

        stop_on_user_request( );

        fsc2_usleep( 20000, UNSET );

        lecroy9400_get_desc( ch );
        for ( cur_avg = 0, j = 0; j < 4; j++ )
            cur_avg = cur_avg * 256 +
                ( long ) lecroy9400.wv_desc[ ch ][ 102 + j ];

        if ( cur_avg >= lecroy9400.num_avg[ ch ] )
            break;
    }

    *length = lecroy9400.rec_len[ ch ];

    len = 2 * *length + 10;
    data = T_malloc( len );

    snprintf( cmd, 100, "RD,%s.DA,1,%ld,0,0\n",
              ch == LECROY9400_FUNC_E ? "FE" : "FF", *length );
    lecroy9400_talk( cmd, ( char * ) data, &len );

    *length = ( ( long ) data[ 2 ] * 256 + ( long ) data[ 3 ] ) / 2;

    /* Make sure we didn't get less data than the header of the data set
       tells us. */

    if ( len < 2 * *length + 4 )
    {
        is_acquiring = UNSET;
        T_free( data );
        print( FATAL, "Received invalid data from digitizer.\n" );
        THROW( EXCEPTION );
    }

    TRY
    {
        *array = T_malloc( *length * sizeof **array );
        TRY_SUCCESS;
    }
    CATCH( OUT_OF_MEMORY_EXCEPTION )
    {
        is_acquiring = UNSET;
        T_free( data );
        RETHROW( );
    }

    gain_fac = sset[ ( int ) lecroy9400.wv_desc[ ch ][ 4 ] - 22 ];
    vgain_fac = 200.0 / ( ( int ) lecroy9400.wv_desc[ ch ][ 5 ] + 80 );
    offset_shift = 0.04
               * ( ( double ) lecroy9400.wv_desc[ ch ][ 8 ] * 256.0
                   + ( double ) lecroy9400.wv_desc[ ch ][ 9 ] - 200 );

    for ( dp = data + 4, i = 0; i < *length; dp += 2, i++ )
        *( *array + i ) = ( double ) dp[ 0 ] * 256.0 + ( double ) dp[ 1 ];

    for ( i = 0; i < *length; i++ )
        *( *array + i ) = gain_fac * ( ( *( *array + i ) - 32768.0 ) / 8192.0
                                       - offset_shift ) * vgain_fac;

    T_free( data );
    is_acquiring = UNSET;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void lecroy9400_free_running( void )
{
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

bool
lecroy9400_command( const char * cmd )
{
    if ( lecroy9400_write( cmd, strlen( cmd ) ) == FAIL )
        lecroy9400_comm_failure( );
    return OK;
}


/*--------------------------------------------------------------*
 *--------------------------------------------------------------*/

static bool
lecroy9400_talk( const char * cmd,
                 char *       reply,
                 long *       length )
{
    if (    lecroy9400_write( cmd, strlen( cmd ) ) == FAIL
         || lecroy9400_read( reply, length ) == FAIL )
        lecroy9400_comm_failure( );
    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static bool
lecroy9400_serial_init( void )
{
    /* We need exclussive access to the serial port and we also need
       non-blocking mode to avoid hanging indefinitely if the other
       side does not react. O_NOCTTY is set because the serial port
       should not become the controlling terminal, otherwise line
       noise read as a CTRL-C might kill the program. */

    if ( ( lecroy9400.tio = fsc2_serial_open( lecroy9400.device,
                          O_RDWR | O_EXCL | O_NOCTTY | O_NONBLOCK ) ) == NULL )
        return FAIL;

    /* Use 8-N-1, allow flow control, ignore modem lines, enable
       reading and set the baud rate. */

    lecroy9400.tio->c_cflag &= ~ ( PARENB | CSTOPB | CSIZE );
    lecroy9400.tio->c_cflag |= CS8 | CLOCAL | CREAD;
    cfsetispeed( lecroy9400.tio, SERIAL_BAUDRATE );
    cfsetospeed( lecroy9400.tio, SERIAL_BAUDRATE );

    lecroy9400.tio->c_iflag = 0;
    lecroy9400.tio->c_oflag = 0;
    lecroy9400.tio->c_lflag = 0;

    fsc2_tcflush( lecroy9400.device, TCIOFLUSH );
    fsc2_tcsetattr( lecroy9400.device, TCSANOW, lecroy9400.tio );

    /* Set up the device to not echo characters */

    if ( lecroy9400_write( "\x1B[", 2 ) == FAIL )
        return FAIL;

    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static bool
lecroy9400_write( const char * buffer,
                  long         length )
{
    if ( fsc2_serial_write( lecroy9400.device, buffer, length,
                            0, SET ) != length )
    {
        if ( length == 0 )
            stop_on_user_request( );
        return FAIL;
    }
    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static bool
lecroy9400_read( char * buffer,
                 long * length )
{
    if ( ( *length = fsc2_serial_read( lecroy9400.device, buffer, *length, NULL,
                                       0, UNSET ) ) <= 0 )
    {
        if ( *length == 0 )
            stop_on_user_request( );

        *length = 0;
        return FAIL;
    }
    return OK;
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

static void
lecroy9400_local( void )
{
    lecroy9400_write( "\x1BL", 2 );
}


/*-----------------------------------------------------------------*
 *-----------------------------------------------------------------*/

void
lecroy9400_comm_failure( void )
{
    is_acquiring = UNSET;

    print( FATAL, "Communication with device failed.\n" );
    THROW( EXCEPTION );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
