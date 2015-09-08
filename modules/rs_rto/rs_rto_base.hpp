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


#pragma once
#if ! defined rs_rto_base_hpp_
#define rs_rto_base_hpp_


#include <vector>
#include "VXI11_Client.hpp"
#include "Enum_Mapper.hpp"
#include "except.hpp"


namespace rs_rto
{

enum class Model : int           // Model of the device
{
    RTO1002,
    RTO1004,
    RTO1012,
    RTO1014,
    RTO1022,
    RTO1024,
    RTO1044
};

enum class Option : int
{
    B1,
    B4,
    B10,
    B18,
    B19,
    B101,
    B102,
    B103,
    B104,
    B200,
    B201,
    B202,
    B203,
    B205,
    K1,
    K2,
    K3,
    K4,
    K5,
    K6,
    K7,
    K8,
    K9,
    K11,
    K12,
    K13,
    K17,
    K21,
    K22,
    K23,
    K24,
    K26,
    K31,
    K40,
    K50,
    K52,
    K55,
    K60,
    U1
};

enum class Channel : int
{
    Ext   = 0,
    Ch1   = 1,
    Ch2   = 2,
    Ch3   = 3,
    Ch4   = 4,
    Math1 = 5,
    Math2 = 6,
    Math3 = 7,
    Math4 = 8
};

enum class Trig_Mode
{
    Auto,
    Normal,
    Free_Running
};

enum class Trig_Slope
{
    Negative,
    Positive,
    Either
};

enum class Coupling
{
    DC50,
    DC1M,
    AC
};

enum class Bandwidth
{
    Full,
    MHz20,
    MHz200,
    MHz800
};

enum class Polarity
{
    Positive,
    Negative
};

enum class Arith_Mode
{
    Off,
    Average,
    Envelope
};

enum class Acq_Mode
{
    Normal,
    Average,
    Segmented
};

enum class Filter_Type
{
    Off,
    Low_Pass,
    High_Pass
};

enum class Filter_Cut_Off
{
    kHz5,
    kHz50,
    MHz50
};

struct Data_Header
{
    double start;
    double end;
    size_t length;
    int    num_values;
};


enum class Binary_Format
{
    IEEE754_LSBF,
    IEEE754_MSBF,
    Other
};


enum class Download_Mode
{
    Waveform,
    Zoom,
    Cursor,
    Gate,
    Manual
};


/*----------------------------------------------------*
 * Base class for the rs_rto class - takes responsibility
 * of interacting with the device and su[llies a number of
 * utility methods mostlty used by the sub-system classes.
 *----------------------------------------------------*/

class rs_rto_base : public VXI11_Client
{
    friend class rs_rto_trig;
    friend class rs_rto_chans;
    friend class rs_rto_chan;
    friend class rs_rto_time;


  public:

    Model
    model( ) const
    {
        return m_model;
    }

    std::vector< Option >
    options( ) const
    {
        return m_options;
    }

    // Returns if a certain option is available

    bool
    has_option( Option op ) const;

    // Methods needed by the C interface                                        

    std::string const &
    c_error( ) const
    {
        return m_c_error;
    }

    int
    num_channels( ) const
    {
        return m_num_channels;
    }

    unsigned int
    max_memory( ) const
    {
        return m_max_memory;
    }

    void
    set_c_error( std::string const & mess ) const
    {
        m_c_error = mess;
    }


  protected:

    rs_rto_base( std::string const & ip_address,
                 Log_Level           log_level = Log_Level::Normal );

	virtual
	~rs_rto_base( )
	{ }

    void
    set_formats( );

    bool
    write( std::string const & data );

    bool
    read( std::string   & data,
          unsigned long   max_len = 0 );

    bool
    read_eos( std::string & data );

    void
    talk( std::string const & cmd,
          std::string       & reply );

    void
    poll_opc( );

    template< typename T >
    T
    query( std::string const & cmd );

    // Default timeout for both reading and writing - seems to be long
    // enough for all "normal" commands. For data transfers we need to
    // know the amount of data and then use an approximate transfer rate

    double const m_def_timeout   = 0.2;     // 200 ms
    double const m_transfer_rate = 1e7;     // 10 MB/s

    static Enum_Mapper< Channel > const s_channel_mapper;

  private :

    void
    get_model( );

    void
    get_options( );

    int
    get_num_channels( );

    unsigned long
    get_max_memory( );

    double
    get_IEEE_float_32( unsigned char const * buf );

    Model m_model;

    std::vector< Option > m_options;

    int m_num_channels;

    unsigned long m_max_memory;

    Binary_Format m_binary_format = Binary_Format::Other;

    mutable std::string m_c_error;
};

}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
