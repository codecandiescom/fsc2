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


/* C wrapper */


#include <algorithm>
#include "rs_rto_c.hpp"

using namespace rs_rto;


static
Enum_Mapper< Log_Level > const log_level_mapper( 
    std::map< int, Log_Level >( { { Log_Level_None,   Log_Level::None   },
                                  { Log_Level_Low,    Log_Level::Low    },
                                  { Log_Level_Normal, Log_Level::Normal },
                                  { Log_Level_High,   Log_Level::High   } } ),
    "log level" );

static
Enum_Mapper< Channel > const channel_mapper( 
    std::map< int, Channel >( { { Channel_Ext,   Channel::Ext   },
                                { Channel_Ch1,   Channel::Ch1   },
                                { Channel_Ch2,   Channel::Ch2   },
                                { Channel_Ch3,   Channel::Ch3   },
                                { Channel_Ch4,   Channel::Ch4   },
                                { Channel_Math1, Channel::Math1 },
                                { Channel_Math2, Channel::Math2 },
                                { Channel_Math3, Channel::Math3 },
                                { Channel_Math4, Channel::Math4 } } ),
    "channel" );

static
Enum_Mapper< Model > const model_mapper( 
    std::map< int, Model >( { { Model_RTO1002, Model::RTO1002 },
                            { Model_RTO1004, Model::RTO1004 },
                            { Model_RTO1012, Model::RTO1012 },
                            { Model_RTO1014, Model::RTO1014 },
                            { Model_RTO1022, Model::RTO1022 },
                            { Model_RTO1024, Model::RTO1024 },
                            { Model_RTO1044, Model::RTO1044 } } ),
    "model" );

static
Enum_Mapper< Option > const option_mapper(
    std::map< int, Option >( { { Option_B1,   Option::B1   },
                               { Option_B4,   Option::B4   },
                               { Option_B10,  Option::B10  },
                               { Option_B18,  Option::B18  },
                               { Option_B19,  Option::B19  },
                               { Option_B101, Option::B101 },
                               { Option_B102, Option::B102 },
                               { Option_B103, Option::B103 },
                               { Option_B104, Option::B104 },
                               { Option_B200, Option::B200 },
                               { Option_B201, Option::B201 },
                               { Option_B202, Option::B202 },
                               { Option_B203, Option::B203 },
                               { Option_B205, Option::B205 },
                               { Option_K1,   Option::K1   },
                               { Option_K2,   Option::K2   },
                               { Option_K3,   Option::K3   },
                               { Option_K4,   Option::K4   },
                               { Option_K5,   Option::K5   },
                               { Option_K6,   Option::K6   },
                               { Option_K7,   Option::K7   },
                               { Option_K8,   Option::K8   },
                               { Option_K9,   Option::K9   },
                               { Option_K11,  Option::K11  },
                               { Option_K12,  Option::K12  },
                               { Option_K13,  Option::K13  },
                               { Option_K17,  Option::K17  },
                               { Option_K21,  Option::K21  },
                               { Option_K22,  Option::K22  },
                               { Option_K23,  Option::K23  },
                               { Option_K24,  Option::K24  },
                               { Option_K26,  Option::K26  },
                               { Option_K31,  Option::K31  },
                               { Option_K40,  Option::K40  },
                               { Option_K50,  Option::K50  },
                               { Option_K52,  Option::K52  },
                               { Option_K55,  Option::K55  },
                               { Option_K60,  Option::K60  },
                               { Option_U1,   Option::U1   } } ),
    "option" );

static
Enum_Mapper< Trig_Mode > const trig_mode_mapper(
    std::map< int, Trig_Mode >(
                      { { Trig_Mode_Auto,         Trig_Mode::Auto         },
                        { Trig_Mode_Normal,       Trig_Mode::Normal       },
                        { Trig_Mode_Free_Running, Trig_Mode::Free_Running } } ),
    "trigger mode" );

static
Enum_Mapper< Trig_Slope > const trig_slope_mapper(
    std::map< int, Trig_Slope >(
                       { { Trig_Slope_Positive, Trig_Slope::Positive },
                         { Trig_Slope_Negative, Trig_Slope::Negative },
                         { Trig_Slope_Either,   Trig_Slope::Either    } } ),
    "trigger slope" );

static
Enum_Mapper< Coupling > const coupling_mapper(
    std::map< int, Coupling >( { { Coupling_DC50, Coupling::DC50 },
                                 { Coupling_DC1M, Coupling::DC1M },
                                 { Coupling_AC,   Coupling::AC   } } ),
    "coupling" );

static
Enum_Mapper< Polarity > const polarity_mapper(
    std::map< int, Polarity >( { { Polarity_Positive, Polarity::Positive },
                               { Polarity_Negative, Polarity::Negative } } ),
    "polarity" );

static
Enum_Mapper< Bandwidth > const bandwidth_mapper( 
    std::map< int, Bandwidth >( { { Bandwidth_Full,   Bandwidth::Full   },
                                  { Bandwidth_MHz20,  Bandwidth::MHz20  },
                                  { Bandwidth_MHz200, Bandwidth::MHz200 },
                                  { Bandwidth_MHz800, Bandwidth::MHz800 } } ),
    "bandwidth" );

static
Enum_Mapper< Acq_Mode > const acq_mode_mapper(
    std::map< int, Acq_Mode >(
                          { { Acq_Mode_Normal,    Acq_Mode::Normal    },
                            { Acq_Mode_Average,   Acq_Mode::Average   },
                            { Acq_Mode_Segmented, Acq_Mode::Segmented } } ),
    "acquisition mode" );

static
Enum_Mapper< Filter_Type > const filter_type_mapper(
    std::map< int, Filter_Type >(
                      { { Filter_Type_Off, Filter_Type::Off },
                        { Filter_Type_Low_Pass, Filter_Type::Low_Pass },
                        { Filter_Type_High_Pass, Filter_Type::High_Pass } } ),
    "filter type" );

static
Enum_Mapper< Filter_Cut_Off > const filter_cut_off_mapper(
    std::map< int, Filter_Cut_Off >(
                      { { Filter_Cut_Off_kHz5,  Filter_Cut_Off::kHz5  },
                        { Filter_Cut_Off_kHz50, Filter_Cut_Off::kHz50 },
                        { Filter_Cut_Off_MHz50, Filter_Cut_Off::MHz50 } } ),
    "filter cut off" );

// I know, nobody likes macros in C++, but this would be difficult to
// do otherwise...

#define CATCH( )                               \
    catch ( std::invalid_argument const & e ) \
    {                                         \
       rs->set_c_error( e.what( ) );          \
       return FSC3_INVALID_ARG;           \
    }                                         \
    catch ( comm_failure const & e )          \
    {                                         \
       rs->set_c_error( e.what( ) );          \
       return FSC3_COMM_FAILURE;          \
    }                                         \
    catch ( bad_data const & e )              \
    {                                         \
       rs->set_c_error( e.what( ) );          \
       return FSC3_BAD_DATA;              \
    }                                         \
    catch ( operational_error const & e )     \
    {                                         \
       rs->set_c_error( e.what( ) );          \
       return FSC3_OP_ERROR;              \
    }                                         \
    catch ( std::bad_alloc const & e )        \
    {                                         \
       rs->set_c_error( "Out of memory" );    \
       return FSC3_OUT_OF_MEMORY;     \
    }                                         \
    catch ( std::exception const & e )        \
    {                                         \
       rs->set_c_error( e.what( ) );          \
       return FSC3_OTHER_ERROR;        \
    }                                  \
    return FSC3_SUCCESS


#define ToCh( ch ) rs->chans[ channel_mapper.v2e( ch ) ]


/*----------------------------------------------------*
 * Helper class that has only one function: breaking out of
 * the namespace the whole thing is living in under C++, but
 * which gets in the way when using C. This works because by
 * creating a class outside of the namespace that just is
 * derived from the original class within the namespace. And
 * this class, outside of the namespace can be used from C
 * since we can create a typedef to it.
 *----------------------------------------------------*/

struct RS_RTO_no_namespace : public RS_RTO
{
    RS_RTO_no_namespace( char const * ip_address,
                         int          log_level )
        : RS_RTO( ip_address, log_level_mapper.v2e( log_level ) )
    { }
};


/*----------------------------------------------------*
 *----------------------------------------------------*/

static
void
check_p( void const * p )
{
    if ( ! p )
        throw std::invalid_argument( "NULL pointer passed as argument" );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

rs_rto_t *
rs_rto_open( char   const  * ip,
			 int             log_level,
             char         ** error_str )
{
    try
    {
        return new RS_RTO_no_namespace( ip, log_level );
    }
    catch ( std::exception const & e )
    {
        if ( error_str )
            *error_str = strdup( e.what( ) );
        return nullptr;
    }
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_close(  rs_rto_t * rs )
{
    try
    {
        check_p( rs );
        delete rs;
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_model( rs_rto_t const * rs,
			  int            * model )
{
    try
    {
        check_p( rs );
        check_p( model );
        *model = model_mapper.e2v( rs->model( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_options( rs_rto_t const  * rs,
				int            ** options,
				size_t          * len )
{
    try
    {
        check_p( rs );
        check_p( options );
        check_p( len );
        auto ov = rs->options( );
        *len = ov.size( );
        if ( ! ( *options = ( int * ) malloc( *len * sizeof **options ) ) )
            throw std::bad_alloc( );
        for ( size_t i = 0; i < *len; ++i )
            ( *options )[ i ] = option_mapper.e2v( ov[ i ] );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_has_option( rs_rto_t const * rs,
				   int              option,
				   bool           * is_available )
{
    try
    {
        check_p( rs );
        *is_available = rs->has_option( option_mapper.v2e( option ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_name(  rs_rto_t const  * rs,
              char     const ** name )
{
    try
    {
        check_p( rs );
        check_p( name );
        *name = rs->name( ).c_str( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_num_channels( rs_rto_t const * rs,
                     int            * num )
{
    try
    {
        check_p( rs );
        check_p( num );
        *num = rs->num_channels( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_max_memory( rs_rto_t const * rs,
                   unsigned long  * size )
{
    try
    {
        check_p( rs );
        check_p( size );
        *size = rs->max_memory( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_display_enabled( rs_rto_t const * rs,
                        bool           * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = rs->display_enabled( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_set_display_enabled( rs_rto_t * rs,
                            bool     * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = rs->set_display_enabled( *state );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_keyboard_locked( rs_rto_t const * rs,
                        bool           * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = rs->keyboard_locked( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_set_keyboard_locked( rs_rto_t * rs,
                            bool     * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = rs->set_keyboard_locked( *state );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_mode( rs_rto_t const * rs,
                     int            * mode )
{
    try
    {
        check_p( rs );
        check_p( mode );
        *mode = trig_mode_mapper.e2v( rs->trig.mode( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_mode( rs_rto_t * rs,
                         int      * mode )
{
    try
    {
        check_p( rs );
        check_p( mode );
        *mode = trig_mode_mapper.e2v( rs->trig.set_mode(
                                             trig_mode_mapper.v2e( *mode ) ) );
    }
    CATCH( );
}
        

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_source( rs_rto_t const * rs,
                       int            * source )
{
    try
    {
        check_p( rs );
        check_p( source );
        *source = channel_mapper.e2v( rs->trig.source( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_source( rs_rto_t * rs,
                           int      * source )
{
    try
    {
        check_p( rs );
        check_p( source );
        *source = channel_mapper.e2v( rs->trig.set_source(
                                   channel_mapper.v2e( *source,
                                                       "trigger channel" ) ) );
    }
    CATCH( );
}
        

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_slope( rs_rto_t const * rs,
                      int            * slope )
{
    try
    {
        check_p( rs );
        check_p( slope );
        *slope = trig_slope_mapper.e2v( rs->trig.slope( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_slope( rs_rto_t * rs,
                          int      * slope )
{
    try
    {
        check_p( rs );
        check_p( slope );
        *slope = trig_slope_mapper.e2v( rs->trig.set_slope(
                                            trig_slope_mapper.v2e( *slope ) ) );
    }
    CATCH( );
}
        

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_channel_slope( rs_rto_t const * rs,
                              int              channel,
                              int            * slope )
{
    try
    {
        check_p( rs );
        check_p( slope );
        *slope = trig_slope_mapper.e2v(
                     rs->trig.slope(
                         channel_mapper.v2e( channel,
                                             "trigger channel" ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_channel_slope( rs_rto_t * rs,
                                  int        channel,
                                  int      * slope )
{
    try
    {
        check_p( rs );
        check_p( slope );
        *slope = trig_slope_mapper.e2v(
            rs->trig.set_slope( channel_mapper.v2e( channel,
                                                    "trigger channel" ),
                                trig_slope_mapper.v2e( *slope ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_level( rs_rto_t * rs,
                      double   * level )
{
    try
    {
        check_p( rs );
        check_p( level );
        *level = rs->trig.level( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_level( rs_rto_t * rs,
                          double   * level )
{
    try
    {
        check_p( rs );
        check_p( level );
        *level = rs->trig.set_level( *level );
    }
    CATCH( );
}
        

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_channel_level( rs_rto_t * rs,
                              int        channel,
                              double   * level )
{
    try
    {
        check_p( rs );
        check_p( level );
        *level = rs->trig.level( channel_mapper.v2e( channel,
                                                     "trigger channel" ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_channel_level( rs_rto_t * rs,
                                  int        channel,
                                  double   * level )
{
    try
    {
        check_p( rs );
        check_p( level );
        *level = rs->trig.set_level( channel_mapper.v2e( channel,
                                                         "trigger channel" ),
                                     *level );
    }
    CATCH( );
}
        

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_min_level( rs_rto_t * rs,
                          double   * min_level )
{
    try
    {
        check_p( rs );
        check_p( min_level );
        *min_level = rs->trig.min_level( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_channel_min_level( rs_rto_t * rs,
                                  int        channel,
                                  double   * min_level )
{
    try
    {
        check_p( rs );
        check_p( min_level );
        *min_level = rs->trig.min_level( channel_mapper.v2e( channel,
                                                   "trigger channel" ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_max_level( rs_rto_t * rs,
                          double   * max_level )
{
    try
    {
        check_p( rs );
        check_p( max_level );
        *max_level = rs->trig.max_level( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_channel_max_level( rs_rto_t * rs,
                                  int        channel,
                                  double   * max_level )
{
    try
    {
        check_p( rs );
        check_p( max_level );
        *max_level = rs->trig.max_level( channel_mapper.v2e( channel,
                                                   "trigger channel" ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_is_overloaded( rs_rto_t * rs,
                              bool     * is_overloaded )
{
    try
    {
        check_p( rs );
        check_p( is_overloaded );
        *is_overloaded = rs->trig.is_overloaded( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_position( rs_rto_t * rs,
                         double   * pos )
{
    try
    {
        check_p( rs );
        check_p( pos );
        *pos = rs->trig.position( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_position( rs_rto_t * rs,
                             double   * pos )
{
    try
    {
        check_p( rs );
        check_p( pos );
        *pos = rs->trig.set_position( *pos );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_earliest_position( rs_rto_t * rs,
                                  double   * min_pos )
{
    try
    {
        check_p( rs );
        check_p( min_pos );
        *min_pos = rs->trig.earliest_position( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_latest_position( rs_rto_t * rs,
                                double   * max_pos )
{
    try
    {
        check_p( rs );
        check_p( max_pos );
        *max_pos = rs->trig.latest_position( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_out_pulse_state( rs_rto_t const * rs,
                                bool           * trig_out_enabled )
{
    try
    {
        check_p( rs );
        check_p( trig_out_enabled );
        *trig_out_enabled = rs->trig.out_pulse_state( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_out_pulse_state( rs_rto_t * rs,
                                    bool     * trig_out_enable )
{
    try
    {
        check_p( rs );
        check_p( trig_out_enable );
        *trig_out_enable = rs->trig.set_out_pulse_state( *trig_out_enable );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_out_pulse_polarity( rs_rto_t const * rs,
                                   int            * pol )
{
    try
    {
        check_p( rs );
        check_p( pol );
        *pol = polarity_mapper.e2v( rs->trig.out_pulse_polarity( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_out_pulse_polarity( rs_rto_t * rs,
                                       int      * pol )
{
    try
    {
        check_p( rs );
        check_p( pol );
        *pol = polarity_mapper.e2v( 
              rs->trig.set_out_pulse_polarity( polarity_mapper.v2e( *pol ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_out_pulse_length( rs_rto_t const * rs,
                                 double         * len )
{
    try
    {
        check_p( rs );
        check_p( len );
        *len = rs->trig.out_pulse_length( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_out_pulse_length( rs_rto_t * rs,
                                     double   * len )
{
    try
    {
        check_p( rs );
        check_p( len );
        *len = rs->trig.set_out_pulse_length( *len );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_out_pulse_delay( rs_rto_t * rs,
                                double   * delay )
{
    try
    {
        check_p( rs );
        check_p( delay );
        *delay = rs->trig.out_pulse_delay( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_set_out_pulse_delay( rs_rto_t * rs,
                                    double   * delay )
{
    try
    {
        check_p( rs );
        check_p( delay );
        *delay = rs->trig.set_out_pulse_delay( *delay );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_min_out_pulse_delay( rs_rto_t * rs,
                                    double   * min_delay )
{
    try
    {
        check_p( rs );
        check_p( min_delay );
        *min_delay = rs->trig.min_out_pulse_delay( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_max_out_pulse_delay( rs_rto_t * rs,
                                    double   * max_delay )
{
    try
    {
        check_p( rs );
        check_p( max_delay );
        *max_delay = rs->trig.max_out_pulse_delay( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_trigger_raise( rs_rto_t * rs )
{
    try
    {
        check_p( rs );
        rs->trig.raise( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_timebase( rs_rto_t * rs,
                     double   * timebase )
{
    try
    {
        check_p( rs );
        check_p( timebase );
        *timebase = rs->acq.timebase( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_timebase( rs_rto_t * rs,
                         double   * timebase )
{
    try
    {
        check_p( rs );
        check_p( timebase );
        *timebase = rs->acq.set_timebase( *timebase );
    }
    CATCH( );
}

/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_shortest_timebase( rs_rto_t const * rs,
                              double         * shortest )
{
    try
    {
        check_p( rs );
        check_p( shortest );
        *shortest = rs->acq.shortest_timebase( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_longest_timebase( rs_rto_t const * rs,
                             double         * longest )
{
    try
    {
        check_p( rs );
        check_p( longest );
        *longest = rs->acq.longest_timebase( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_shortest_timebase_const_resolution( rs_rto_t const * rs,
                                               double         * shortest )
{
    try
    {
        check_p( rs );
        check_p( shortest );
        *shortest = rs->acq.shortest_timebase_const_resolution( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_longest_timebase_const_resolution( rs_rto_t const * rs,
                                              double         * longest )
{
    try
    {
        check_p( rs );
        check_p( longest );
        *longest = rs->acq.longest_timebase_const_resolution( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_record_length( rs_rto_t      * rs,
                          unsigned long * record_length )
{
    try
    {
        check_p( rs );
        check_p( record_length );
        *record_length = rs->acq.record_length( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_record_length( rs_rto_t      * rs,
                              unsigned long * record_length )
{
    try
    {
        check_p( rs );
        check_p( record_length );
        *record_length = rs->acq.set_record_length( *record_length );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_min_record_length( rs_rto_t const * rs,
                              unsigned long  * min_record_length )
{
    try
    {
        check_p( rs );
        check_p( min_record_length );
        *min_record_length = rs->acq.min_record_length( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_max_record_length( rs_rto_t      * rs,
                              unsigned long * max_record_length )
{
    try
    {
        check_p( rs );
        check_p( max_record_length );
        *max_record_length = rs->acq.max_record_length( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_resolution( rs_rto_t * rs,
                       double   * resolution )
{
    try
    {
        check_p( rs );
        check_p( resolution );
        *resolution = rs->acq.resolution( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_resolution( rs_rto_t * rs,
                           double   * resolution )
{
    try
    {
        check_p( rs );
        check_p( resolution );
        *resolution = rs->acq.set_resolution( *resolution );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_lowest_resolution( rs_rto_t * rs,
                              double   * lowest )
{
    try
    {
        check_p( rs );
        check_p( lowest );
        *lowest = rs->acq.lowest_resolution( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_highest_resolution( rs_rto_t * rs,
                               double   * highest )
{
    try
    {
        check_p( rs );
        check_p( highest );
        *highest = rs->acq.highest_resolution( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_average_count( rs_rto_t const * rs,
                          unsigned long  * count )
{
    try
    {
        check_p( rs );
        check_p( count );
        *count = rs->acq.average_count( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_average_count( rs_rto_t      * rs,
                              unsigned long * count )
{
    try
    {
        check_p( rs );
        check_p( count );
        *count = rs->acq.set_average_count( *count );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_max_average_count( rs_rto_t const * rs,
                              unsigned long  * max_count )
{
    try
    {
        check_p( rs );
        check_p( max_count );
        *max_count = rs->acq.max_average_count( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_segment_count( rs_rto_t const * rs,
                          unsigned long  * count )
{
    try
    {
        check_p( rs );
        check_p( count );
        *count = rs->acq.segment_count( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_segment_count( rs_rto_t      * rs,
                              unsigned long * count )
{
    try
    {
        check_p( rs );
        check_p( count );
        *count = rs->acq.set_segment_count( *count );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_max_segment_count( rs_rto_t const * rs,
                              unsigned long  * max_count )
{
    try
    {
        check_p( rs );
        check_p( max_count );
        *max_count = rs->acq.max_segment_count( );
    }
    CATCH( );
}



/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_reference_position( rs_rto_t * rs,
                               double   * pos )
{
    try
    {
        check_p( rs );
        check_p( pos );
        *pos = rs->acq.reference_position( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_reference_position( rs_rto_t * rs,
                                   double   * pos )
{
    try
    {
        check_p( rs );
        check_p( pos );
        *pos = rs->acq.set_reference_position( *pos );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_mode( rs_rto_t const * rs,
                 int            * mode )
{
    try
    {
        check_p( rs );
        check_p( mode );
        *mode = acq_mode_mapper.e2v( rs->acq.mode( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_mode( rs_rto_t * rs,
                     int      * mode )
{
    try
    {
        check_p( rs );
        check_p( mode );
        *mode = acq_mode_mapper.e2v( rs->acq.set_mode(
                                           acq_mode_mapper.v2e( *mode ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_is_running( rs_rto_t * rs,
                       bool     * is_running )
{
    try
    {
        check_p( rs );
        check_p( is_running );
        *is_running = rs->acq.is_running( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_is_waiting_for_trigger( rs_rto_t * rs,
                                   bool     * is_waiting )
{
    try
    {
        check_p( rs );
        check_p( is_waiting );
        *is_waiting = rs->acq.is_waiting_for_trigger( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_run( rs_rto_t * rs )
{
    try
    {
        check_p( rs );
        rs->acq.run( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_run_single( rs_rto_t * rs )
{
    try
    {
        check_p( rs );
        rs->acq.run_single( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_run_continuous( rs_rto_t * rs )
{
    try
    {
        check_p( rs );
        rs->acq.run_continuous( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_stop( rs_rto_t * rs,
                 bool     * was_running )
{
    try
    {
        check_p( rs );
        bool ws = rs->acq.stop( );
        if ( was_running )
            *was_running = ws;
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_available_segments( rs_rto_t      * rs,
                               unsigned long * count )
{
    try
    {
        check_p( rs );
        check_p( count );
        *count = rs->acq.available_segments( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_download_limits_enabled( rs_rto_t * rs,
                                    bool     * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = rs->acq.download_limits_enabled( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_download_limits_enabled( rs_rto_t * rs,
                                        bool     * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = rs->acq.set_download_limits_enabled( *state );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_download_limits( rs_rto_t * rs,
                            double   * start,
                            double   * end )
{
    try
    {
        check_p( rs );
        check_p( start );
        check_p( end );
        std::vector< double > l = rs->acq.download_limits( );
        *start = l[ 0 ];
        *end   = l[ 1 ];
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_set_download_limits( rs_rto_t * rs,
                                double   * start,
                                double   * end )
{
    try
    {
        check_p( rs );
        check_p( start );
        check_p( end );
        std::vector< double > l = rs->acq.set_download_limits( *start, *end );
        *start = l[ 0 ];
        *end   = l[ 1 ];
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_acq_max_download_limits( rs_rto_t * rs,
                                double   * start,
                                double   * end )
{
    try
    {
        check_p( rs );
        check_p( start );
        check_p( end );
        std::vector< double > l = rs->acq.max_download_limits( );
        *start = l[ 0 ];
        *end   = l[ 1 ];
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_exists( rs_rto_t const * rs,
                       int              ch,
                       bool           * exists )
{
    try
    {
        check_p( rs );
        check_p( exists );
        *exists = ToCh( ch ).exists( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_name( rs_rto_t const  * rs,
                     int               ch,
                     char           ** name )
{
    try
    {
        check_p( rs );
        check_p( name );
        if ( ! ( *name = strdup( ToCh( ch ).name( "c" ).c_str( ) ) ) )
            throw std::bad_alloc( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_state( rs_rto_t * rs,
                      int        ch,
                      bool     * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = ToCh( ch ).state( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_state( rs_rto_t * rs,
                          int        ch,
                          bool     * state )
{
    try
    {
        check_p( rs );
        check_p( state );
        *state = ToCh( ch ).set_state( *state );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_is_overloaded( rs_rto_t * rs,
                              int        ch,
                              bool     * is_overloaded )
{
    try
    {
        check_p( rs );
        check_p( is_overloaded );
        *is_overloaded = ToCh( ch ).is_overloaded( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_coupling( rs_rto_t * rs,
                         int        ch,
                         int      * coup )
{
    try
    {
        check_p( rs );
        check_p( coup );
        *coup = coupling_mapper.e2v( ToCh( ch ).coupling( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_coupling( rs_rto_t * rs,
                             int        ch,
                             int      * coup )
{
    try
    {
        check_p( rs );
        check_p( coup );
        *coup = coupling_mapper.e2v( ToCh( ch ).set_coupling(
                                         coupling_mapper.v2e( *coup ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_scale( rs_rto_t * rs,
                      int        ch,
                      double   * scale )
{
    try
    {
        check_p( rs );
        check_p( scale );
        *scale = ToCh( ch ).scale( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_scale( rs_rto_t * rs,
                          int        ch,
                          double   * scale )
{
    try
    {
        check_p( rs );
        check_p( scale );
        *scale = ToCh( ch ).set_scale( *scale );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_min_scale( rs_rto_t * rs,
                          int        ch,
                          double   * min_scale )
{
    try
    {
        check_p( rs );
        check_p( min_scale );
        *min_scale = ToCh( ch ).min_scale( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_max_scale( rs_rto_t * rs,
                          int        ch,
                          double   * max_scale )
{
    try
    {
        check_p( rs );
        check_p( max_scale );
        *max_scale = ToCh( ch ).max_scale( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_offset( rs_rto_t * rs,
                       int        ch,
                       double   * offset )
{
    try
    {
        check_p( rs );
        check_p( offset );
        *offset = ToCh( ch ).offset( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_offset( rs_rto_t * rs,
                           int        ch,
                           double   * offset )
{
    try
    {
        check_p( rs );
        check_p( offset );
        *offset = ToCh( ch ).set_offset( *offset );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_min_offset( rs_rto_t * rs,
                           int        ch,
                           double   * min_offset )
{
    try
    {
        check_p( rs );
        check_p( min_offset );
        *min_offset = ToCh( ch ).min_offset( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_max_offset( rs_rto_t * rs,
                           int        ch,
                           double   * max_offset )
{
    try
    {
        check_p( rs );
        check_p( max_offset );
        *max_offset = ToCh( ch ).max_offset( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_position( rs_rto_t * rs,
                         int        ch,
                         double   * pos )
{
    try
    {
        check_p( rs );
        check_p( pos );
        *pos = ToCh( ch ).position( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_position( rs_rto_t * rs,
                             int        ch,
                             double   * pos )
{
    try
    {
        check_p( rs );
        check_p( pos );
        *pos = ToCh( ch ).set_position( *pos );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_impedance( rs_rto_t const * rs,
                          int              ch,
                          double         * imp )
{
    try
    {
        check_p( rs );
        check_p( imp );
        *imp = ToCh( ch ).impedance( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_impedance( rs_rto_t * rs,
                              int        ch,
                              double   * imp )
{
    try
    {
        check_p( rs );
        check_p( imp );
        *imp = ToCh( ch ).set_impedance( *imp );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_min_impedance( rs_rto_t const * rs,
                              int              ch,
                              double         * min_imp )
{
    try
    {
        check_p( rs );
        check_p( min_imp );
        *min_imp = ToCh( ch ).min_impedance( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_max_impedance( rs_rto_t const * rs,
                              int              ch,
                              double         * max_imp )
{
    try
    {
        check_p( rs );
        check_p( max_imp );
        *max_imp = ToCh( ch ).max_impedance( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_bandwidth( rs_rto_t const * rs,
                          int              ch,
                          int            * bw )
{
    try
    {
        check_p( rs );
        check_p( bw );
        *bw = bandwidth_mapper.e2v( ToCh( ch ).bandwidth( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_bandwidth( rs_rto_t * rs,
                              int        ch,
                              int      * bw )
{
    try
    {
        check_p( rs );
        check_p( bw );
        *bw = bandwidth_mapper.e2v( ToCh( ch ).set_bandwidth(
                                                bandwidth_mapper.v2e( *bw ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_filter_type( rs_rto_t const * rs,
                            int              ch,
                            int            * ft )
{
    try
    {
        check_p( rs );
        check_p( ft );
        *ft = filter_type_mapper.e2v( ToCh( ch ).filter_type( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_filter_type( rs_rto_t * rs,
                                int        ch,
                                int      * ft )
{
    try
    {
        check_p( rs );
        check_p( ft );
        *ft = filter_type_mapper.e2v( ToCh( ch ).set_filter_type(
                                             filter_type_mapper.v2e( *ft ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_cut_off( rs_rto_t const * rs,
                        int              ch,
                        int            * fco )
{
    try
    {
        check_p( rs );
        check_p( fco );
        *fco = filter_cut_off_mapper.e2v( ToCh( ch ).cut_off( ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_cut_off( rs_rto_t * rs,
                            int        ch,
                            int      * fco )
{
    try
    {
        check_p( rs );
        check_p( fco );
        *fco = filter_cut_off_mapper.e2v( ToCh( ch ).set_cut_off(
                                          filter_cut_off_mapper.v2e( *fco ) ) );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_header( rs_rto_t      * rs,
                       int             ch,
                       Data_Header_t * header )
{
    try
    {
        check_p( rs );
        check_p( header );
        Data_Header dh = ToCh( ch ).header( );
        header->start  = dh.start;
        header->end    = dh.end;
        header->length = dh.length;
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_data( rs_rto_t  * rs,
                     int         ch,
                     double   ** data,
                     size_t    * length )
{
    try
    {
        check_p( rs );
        check_p( data );
        check_p( length );
        std::vector< double > d = ToCh( ch ).data( );

        if ( ! ( *data = ( double * ) malloc( d.size( ) * sizeof **data ) ) )
            throw std::bad_alloc( );

        std::copy( d.begin( ), d.end( ), *data );
        *length = d.size( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_segment_data( rs_rto_t   * rs,
                             int          ch,
                             double   *** data,
                             size_t     * num_segments,
                             size_t     * length )
{
    return rs_rto_channel_segment_data_subset( rs, ch, 0, 0, data,
                                               num_segments, length );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_segment_data_subset( rs_rto_t        * rs,
                                    int               ch,
                                    unsigned long     start_index,
                                    unsigned long     count,
                                    double        *** data,
                                    size_t          * num_segments,
                                    size_t          * length )
{
    try
    {
        check_p( rs );
        check_p( data );
        check_p( num_segments );
        check_p( length );

        std::vector< std::vector< double > > sd =
                                ToCh( ch ).segment_data( start_index, count );

        size_t ns = sd.size( );
        size_t l  = sd[ 0 ].size( );

        if ( ! ( *data = ( double ** ) malloc( ns * sizeof **data ) ) )
            throw std::bad_alloc( );

        if ( ! ( **data = ( double * ) malloc( ns * l * sizeof ***data ) ) )
        {
            free( *data );
            throw std::bad_alloc( );
        }

        std::copy( sd[ 0 ].begin( ), sd[ 0 ].end( ), **data );

        for ( size_t i = 1; i < ns; ++i )
        {
            ( *data )[ i ] = ( *data )[ i - 1 ] + l;
            std::copy( sd[ i ].begin( ), sd[ i ].end( ), ( *data)[ i ] );
        }
        
        *num_segments = ns;
        *length = l;
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_function( rs_rto_t  * rs,
                         int         ch,
                         char     ** func )
{
    try
    {
        check_p( rs );
        check_p( func );
        if ( ! ( *func = strdup( ToCh( ch ).function( ).c_str( ) ) ) )
            throw std::bad_alloc( );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

int
rs_rto_channel_set_function( rs_rto_t   * rs,
                             int          ch,
                             char const * func )
{
    try
    {
        check_p( rs );
        check_p( func );
        ToCh( ch ).set_function( func );
    }
    CATCH( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

char const *
rs_rto_last_error( rs_rto_t const * rs )
{
    if ( ! rs )
        return nullptr;
    return rs->c_error( ).c_str( );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
