/*
 *  Copyright (C) 2015 Jens Thoms Toerring
 *
 *  This file is part of Fsc3.
 *
 *  Fsc3 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 *  Fsc3 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <limits>
#include <cmath>
#include <cstring>
#include "rs_rto_base.hpp"

using namespace rs_rto;

Enum_Mapper< Channel > const rs_rto_base::s_channel_mapper(
    std::map< int, Channel >(
                     { { enum_to_value( Channel::Ext ),   Channel::Ext   },
                       { enum_to_value( Channel::Ch1 ),   Channel::Ch1   },
                       { enum_to_value( Channel::Ch2 ),   Channel::Ch2   },
                       { enum_to_value( Channel::Ch3 ),   Channel::Ch3   },
                       { enum_to_value( Channel::Ch4 ),   Channel::Ch4   },
                       { enum_to_value( Channel::Math1 ), Channel::Math1 },
                       { enum_to_value( Channel::Math2 ), Channel::Math2 },
                       { enum_to_value( Channel::Math3 ), Channel::Math3 },
                       { enum_to_value( Channel::Math4 ), Channel::Math4 } } ),
    "channel" );


namespace rs_rto
{

/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
bool
rs_rto_base::query< bool >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "0" )
        return false;
    else if ( reply == "1" )
        return true;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
int
rs_rto_base::query< int >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    errno = 0;
    char * ep;
    long res = std::strtol( reply.c_str( ), &ep, 10 );
    if (    ep == reply.c_str( )
         || *ep
         || res > std::numeric_limits< int >::max( )
         || res < std::numeric_limits< int >::min( ) )
        throw bad_data( "Invalid reply from device" );

    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
unsigned int
rs_rto_base::query< unsigned int >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    errno = 0;
    char * ep;
    unsigned long res = std::strtoul( reply.c_str( ), &ep, 10 );
    if (    ep == reply.c_str( )
         || *ep
         || res > std::numeric_limits< unsigned int >::max( ) )
        throw bad_data( "Invalid reply from device" );

    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
unsigned long
rs_rto_base::query< unsigned long >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    errno = 0;
    char * ep;
    unsigned long res = std::strtoul( reply.c_str( ), &ep, 10 );
    if (    ep == reply.c_str( )
         || *ep
         || errno == ERANGE )
        throw bad_data( "Invalid reply from device" );

    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
double
rs_rto_base::query< double >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    errno = 0;
    char * ep;
    double res = std::strtod( reply.c_str( ), &ep );
    if ( ep == reply.c_str( ) || *ep || errno == ERANGE )
        throw bad_data( "Invalid reply from device" );

    return res;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
std::vector< double >
rs_rto_base::query< std::vector< double > >( std::string const & cmd )
{
    // Send the command

    write( cmd );

    // Try to read the first two bytes of the reply - they should contain
    // a hash mark and the number of digits in the number of bytes we're
    // going to get

    std::string reply;
    read( reply, 2 );

    if ( reply[ 0 ] == '0' )
        throw operational_error( "No data available" );

    if (    reply[ 0 ] != '#'
         || ! std::isdigit( static_cast< int >( reply[ 1 ] ) ) )
        throw bad_data( "Invalid reply from device" );

    // If the number of digits is 0 this means that there's at least 1 GB
    // of data to be downloaded, but it's not going to transmitted.

    unsigned long lcnt;
    int cnt = reply[ 1 ] - '0';

    if ( cnt == 0 )
    {
        // Since we don't know how many bytes to expect we can only set
        // the read timeout to something that should suffice for even
        // models with the highest possible waveform length, which is
        // 800 MSa (if all memory is used for a single channel). This
        // still has to be multiplied by the 4 bytes each point requires..

        set_default_read_timeout( m_def_timeout + 2.4e9 / m_transfer_rate );
        reply.clear( );
        read_eos( reply );
        set_default_read_timeout( m_def_timeout );

        if ( reply.size( ) % 8 )
            throw bad_data( "Invalid reply from device" );
        lcnt = reply.size( ) / 4;
    }
    else
    {
        // Now read and evaluate the number of bytes we'll have to download

        reply.clear( );
        read( reply, cnt );

        char *ep;
        errno = 0;
        lcnt = strtoul( reply.c_str( ), &ep, 10 );
        if (    ep == reply.c_str( )
             || *ep
             || errno == ERANGE
             || lcnt % 4 )
            throw bad_data( "Invalid reply from device" );

        // There's a firmware bug: even if the device signals that there
        // are no points to be read (in case the export start and stop
        // value are identical) it sends a single point, which we must
        // read in to avoid getting a reply with that data point on our
        // next request.

        if ( lcnt == 0 )
        {
            if ( more_data_available( ) )
                read_eos( reply );
            throw operational_error( "No data available" );
        }

        // Raise the read timeout for reading the data to something that
        // should be long enough. Then read all the data and afterwards
        // reset the timeout to the normal value.

        set_default_read_timeout( m_def_timeout + lcnt / m_transfer_rate );
        reply.clear( );
        read_eos( reply );
        set_default_read_timeout( m_def_timeout );

        if ( reply.size( ) != lcnt )
            throw bad_data( "Invalid reply from device" );

        lcnt /= 4;
    }

    // Convert all of the binary data into an array of doubles. The data
    // are IEEE 754 floats with least significant byte first. If this
    // machine has the same binary float representation we can leave
    // the heavy lifting to what C++ supplies. If the machine also uses
    // IEEE 754 but is big endian, we have to reverse the four bytes of
    // the float. And otherwise we must assemble each point the hard
    // way.

    unsigned char const * rp =
                   reinterpret_cast< unsigned char const * >( reply.c_str( ) );

    if ( m_binary_format == Binary_Format::IEEE754_LSBF )
        return std::vector< double >(
                              reinterpret_cast< float const * >( rp ),
                              reinterpret_cast< float const * >( rp ) + lcnt );
    else if ( m_binary_format == Binary_Format::IEEE754_MSBF )
    {
        std::vector< double > v;

        float tmp;
        unsigned char * tp = reinterpret_cast< unsigned char * >( &tmp ) + 3;

        for ( size_t i = 0; i < lcnt; tp += 3, ++i )
        {
            *tp-- = *rp++;
            *tp-- = *rp++;
            *tp-- = *rp++;
            *tp   = *rp++;

            v.push_back( tmp );
        }

        return v;
    }
    else
    {
        std::vector< double > v;

        for ( size_t i = 0; i < lcnt; rp += 4, ++i )
            v.push_back( get_IEEE_float_32( rp ) );

        return v;
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Trig_Mode
rs_rto_base::query< Trig_Mode >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "AUTO" )
        return Trig_Mode::Auto;
    else if ( reply == "NORM" )
        return Trig_Mode::Normal;
    else if ( reply == "FRE" )
        return Trig_Mode::Free_Running;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Channel
rs_rto_base::query< Channel >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "EXT" )
        return Channel::Ext;
    else if ( reply == "CHAN1" )
        return Channel::Ch1;
    else if ( reply == "CHAN2" )
        return Channel::Ch2;
    else if ( reply == "CHAN3" )
        return Channel::Ch3;
    else if ( reply == "CHAN4" )
        return Channel::Ch4;
    else if ( reply == "MATH1" )
        return Channel::Math1;
    else if ( reply == "MATH2" )
        return Channel::Math2;
    else if ( reply == "MATH3" )
        return Channel::Math3;
    else if ( reply == "MATH4" )
        return Channel::Math4;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Trig_Slope
rs_rto_base::query< Trig_Slope >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "POS" )
        return Trig_Slope::Positive;
    else if ( reply == "NEG" )
        return Trig_Slope::Negative;
    else if ( reply == "EITH" )
        return Trig_Slope::Either;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Coupling
rs_rto_base::query< Coupling >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "DC" )
        return Coupling::DC50;
    else if ( reply == "DCL" )
        return Coupling::DC1M;
    else if ( reply == "AC" )
        return Coupling::AC;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Data_Header
rs_rto_base::query< Data_Header >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    Data_Header header;
    if ( sscanf( reply.c_str( ), "%lf,%lf,%zu,%d",
                 &header.start, &header.end,
                 &header.length, &header.num_values ) == 4 )
        return header;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Bandwidth
rs_rto_base::query< Bandwidth >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "FULL" )
        return Bandwidth::Full;
    else if ( reply == "B20" )
        return Bandwidth::MHz20;
    else if ( reply == "B200" )
        return Bandwidth::MHz200;
    else if ( reply == "B800" )
        return Bandwidth::MHz800;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Polarity
rs_rto_base::query< Polarity >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "POS" )
        return Polarity::Positive;
    else if ( reply == "NEG" )
        return Polarity::Negative;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Arith_Mode
rs_rto_base::query< Arith_Mode >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "OFF" )
        return Arith_Mode::Off;
    else if ( reply == "AVER" )
        return Arith_Mode::Average;
    else if ( reply == "ENV" )
        return Arith_Mode::Envelope;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Filter_Type
rs_rto_base::query< Filter_Type >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "OFF" )
        return Filter_Type::Off;
    else if ( reply == "LFR" )
        return Filter_Type::High_Pass;
    else if ( reply == "RFR" )
        return Filter_Type::Low_Pass;

    throw bad_data( "Invalid reply from device" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

template< >
Filter_Cut_Off
rs_rto_base::query< Filter_Cut_Off >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "KHZ5" )
        return Filter_Cut_Off::kHz5;
    else if ( reply == "KHZ50" )
        return Filter_Cut_Off::kHz50;
    else if ( reply == "MHZ50" )
        return Filter_Cut_Off::MHz50;

    throw bad_data( "Invalid reply from device" );
}


template< >
Download_Mode
rs_rto_base::query< Download_Mode >( std::string const & cmd )
{
    std::string reply;
    talk( cmd, reply );

    if ( reply == "WFM" )
        return Download_Mode::Waveform;
    else if ( reply == "MAN" )
        return Download_Mode::Manual;
    else if ( reply == "ZOOM" )
        return Download_Mode::Zoom;
    else if ( reply == "CURS" )
        return Download_Mode::Cursor;
    else if ( reply == "GATE" )
        return Download_Mode::Gate;

    throw bad_data( "Invalid reply from device" );
}            

};


/*----------------------------------------------------*
 *----------------------------------------------------*/

rs_rto_base::rs_rto_base( std::string const & ip_address,
                          Log_Level           log_level )
    : VXI11_Client( "RS_RTO", ip_address, "INSTR", log_level )
{
    set_default_read_timeout( m_def_timeout );
    set_default_write_timeout( m_def_timeout );

    if ( ! VXI11_Client::connect( ) )
        throw comm_failure( "Failed to connect: " + last_error( ) );

    // Clear the error queue

    std::string reply;
    talk( "SYST:ERR:ALL?", reply );

    get_model( );
    get_options( );
    get_num_channels( );
    get_max_memory( );
    set_formats( );
};


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_base::set_formats( )
{
    // Disable transmission of x values

    write( "EXP:WAV:INCX 0" );

    // Request replies to be binary block data - ASCII transfers would
    // take up to 5 times longer

    write( "FORM REAL,32" );

    // Check if the machine uses IEEE 754 and is little or big endian

    if ( std::numeric_limits< float >::is_iec559 )
    {
        int tst = 0;
        * reinterpret_cast< unsigned char * >( &tst ) = 1;
        if ( tst == 1 )
            m_binary_format = Binary_Format::IEEE754_LSBF;
        else
            m_binary_format = Binary_Format::IEEE754_MSBF;
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_base::has_option( Option op ) const
{
    for ( auto o : m_options )
        if ( o == op )
            return true;
    return false;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_base::write( std::string const & data,
                    bool,
                    double )
{
    if ( ! VXI11_Client::write( data ) )
        throw comm_failure( last_error( ) );
    return true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_base::read( std::string   & data,
                   unsigned long   max_len,
                   double )
{
    if ( ! VXI11_Client::read( data, max_len ) )
        throw comm_failure( last_error( ) );
    return true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

bool
rs_rto_base::read_eos( std::string & data )
{
    if ( ! VXI11_Client::read_eos( data ) )
        throw comm_failure( last_error( ) );

    if ( data.size( ) > 0 )
        data.pop_back( );
    return true;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_base::talk( std::string const & cmd,
                   std::string       & reply )
{
    write( cmd );
    read_eos( reply );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
rs_rto_base::poll_opc( )
{
    while ( ! query< bool >( "*OPC?" ) )
        /* empty */ ;
}        


/*----------------------------------------------------*
 * Method for finding out which model we're talking to
 *----------------------------------------------------*/

void
rs_rto_base::get_model( )
{
    std::vector< std::pair< char const *, Model > > models =
                                       { { "1316.1000k02", Model::RTO1002 },
                                         { "1316.1000k04", Model::RTO1004 },
                                         { "1316.1000k12", Model::RTO1012 },
                                         { "1316.1000k14", Model::RTO1014 },
                                         { "1316.1000k22", Model::RTO1022 },
                                         { "1316.1000k24", Model::RTO1024 },
                                         { "1316.1000k44", Model::RTO1044 } };


    std::string reply;
    talk( "*IDN?", reply );

    for ( auto v : models )
        if ( reply.find( v.first ) != std::string::npos )
        {
            m_model = v.second;
            return;
        }

    bad_data( "No information about model of RTO found" );
}


/*----------------------------------------------------*
 * Method for finding out which options are installed
 *----------------------------------------------------*/

void
rs_rto_base::get_options( )
{
    std::map< std::string, Option > options =
                                       { { "B1",   Option::B1   },
                                         { "B4",   Option::B4   },
                                         { "B10",  Option::B10  },
                                         { "B18",  Option::B18  },
                                         { "B19",  Option::B19  },
                                         { "B101", Option::B101 },
                                         { "B102", Option::B102 },
                                         { "B103", Option::B103 },
                                         { "B104", Option::B104 },
                                         { "B200", Option::B200 },
                                         { "B201", Option::B201 },
                                         { "B202", Option::B202 },
                                         { "B203", Option::B203 },
                                         { "B205", Option::B205 },
                                         { "K1",   Option::K1   },
                                         { "K2",   Option::K2   },
                                         { "K3",   Option::K3   },
                                         { "K4",   Option::K4   },
                                         { "K5",   Option::K5   },
                                         { "K6",   Option::K6   },
                                         { "K7",   Option::K7   },
                                         { "K8",   Option::K8   },
                                         { "K9",   Option::K9   },
                                         { "K11",  Option::K11  },
                                         { "K12",  Option::K12  },
                                         { "K13",  Option::K13  },
                                         { "K17",  Option::K17  },
                                         { "K21",  Option::K21  },
                                         { "K22",  Option::K22  },
                                         { "K23",  Option::K23  },
                                         { "K24",  Option::K24  },
                                         { "K26",  Option::K26  },
                                         { "K31",  Option::K31  },
                                         { "K40",  Option::K40  },
                                         { "K50",  Option::K50  },
                                         { "K52",  Option::K52  },
                                         { "K55",  Option::K55  },
                                         { "K60",  Option::K60  },
                                         { "U1",   Option::U1   } };

    std::string reply;
    talk( "*OPT?", reply );

    char * opt_str = new char[ reply.size( ) + 1 ];
    strcpy( opt_str, reply.c_str( ) );

    char *op = opt_str;

    try
    {
        while ( ( op = std::strtok( op, ", " ) ) )
        {
            auto it = options.find( op );
            if ( it != options.end( ) )
                m_options.push_back( it->second );
            op = nullptr;
        }
    }
    catch ( ... )
    {
        delete [ ] opt_str;
        throw;
    }

    delete [ ] opt_str;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_base::get_num_channels( )
{
    return m_num_channels = query< int >( "DIAG:SERV:CHAN?" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

unsigned long
rs_rto_base::get_max_memory( )
{
    if ( has_option( Option::B104 ) )
    {
        if ( m_model == Model::RTO1002 || m_model == Model::RTO1012 )
            m_max_memory = 400000000;
        else if ( m_model == Model::RTO1022 )
            m_max_memory = 800000000;
        else if ( m_model == Model::RTO1004 || m_model == Model::RTO1014 )
            m_max_memory = 400000000;
        else
            m_max_memory = 800000000;
    }
    else if ( has_option( Option::B103 ) )
    {
        if ( m_model == Model::RTO1002 || m_model == Model::RTO1012 )
            m_max_memory = 200000000;
        else if ( m_model == Model::RTO1022 )
            m_max_memory = 400000000;
        else if ( m_model == Model::RTO1004 || m_model == Model::RTO1014 )
            m_max_memory = 200000000;
        else
            m_max_memory = 800000000;
    }
    else if ( has_option( Option::B102 ) )
    {
        if ( m_model == Model::RTO1002 || m_model == Model::RTO1012 )
            m_max_memory = 100000000;
        else if ( m_model == Model::RTO1022 )
            m_max_memory = 200000000;
        else if ( m_model == Model::RTO1004 || m_model == Model::RTO1014 )
            m_max_memory = 100000000;
        else
            m_max_memory = 400000000;
    }
    else if ( has_option( Option::B101 ) )
    {
        if ( m_model == Model::RTO1002 || m_model == Model::RTO1012 )
            m_max_memory = 50000000;
        else if ( m_model == Model::RTO1022 )
            m_max_memory =100000000;
        else if ( m_model == Model::RTO1004 || m_model == Model::RTO1014 )
            m_max_memory = 50000000;
        else
            m_max_memory = 200000000;
    }
    else
    {
        if ( m_model == Model::RTO1002 || m_model == Model::RTO1012 )
            m_max_memory = 20000000;
        else if ( m_model == Model::RTO1022 )
            m_max_memory =40000000;
        else if ( m_model == Model::RTO1004 || m_model == Model::RTO1014 )
            m_max_memory = 20000000;
        else
            m_max_memory = 80000000;
    }

    return m_max_memory;
}


/*----------------------------------------------------*
 * Function for converting a IEEE 754 float value with
 * least significant byte first to a value in the bit
 * representation on this machine.
 *----------------------------------------------------*/

double
rs_rto_base::get_IEEE_float_32( unsigned char const * buf )
{
    double mant;
	double sign = ( buf[ 3 ] & 0x80 ) ? -1.0 : 1.0;
	unsigned char expo = ( ( ( buf[ 3 ] & 0x7F ) << 1 ) | ( buf[ 2 ] >> 7 ) );

	if ( expo == 0xFF )
        throw bad_data( "Invalid reply from device" );

	if ( expo )
	{
		mant = ( ( buf[ 2 ] | 0x80 ) << 16 ) | ( buf[ 1 ] << 8 ) | buf[ 0 ];
		return sign * ( mant / 0x800000 ) * std::pow( 2.0, expo - 127 );
	}
	else
	{
		mant = ( buf[ 2 ] << 16 ) | ( buf[ 1 ] << 8 ) | buf[ 0 ];
		return sign * ( mant / 0x800000 ) * std::pow( 2.0, -126 );
	}
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
