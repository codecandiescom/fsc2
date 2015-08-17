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


#if ! defined rs_rto_chan_hpp_
#define rs_rto_chan_hpp_


#include "rs_rto.hpp"


#define rs_rto_chan_FAIL( )  throw::std::invalid_argument( fail( __func__ ) )

namespace rs_rto
{

class RS_RTO;


/*----------------------------------------------------*
 * Base class for all channels (if not used as a base class
 * it's an instance for a non-existing channel)
 *----------------------------------------------------*/

class rs_rto_chan
{
	friend class rs_rto_chans;
	friend class rs_rto_acq;


  public:

	virtual
	~rs_rto_chan( ) = default;

	bool
	exists( ) const
	{
		return m_channel_exists;
	}

	std::string
	name( std::string const & lang = "c++" ) const
	{
        if ( lang == "python" )
            return name_py( );
        else if ( lang == "c" )
        {
            std::string n = m_name;
            return n.replace( n.find( "::" ), 2, "_" );
        }

		return m_name;
	}

	std::string
	name_py( ) const
	{
        std::string n = m_name;
		return n.replace( n.find( "::" ), 2, "." );
	}

	// Public methods that may be supported by the channels

	virtual bool state( )                            { rs_rto_chan_FAIL( ); }

	virtual bool set_state( bool )                   { rs_rto_chan_FAIL( ); }

	virtual bool is_overloaded( bool = true )        { rs_rto_chan_FAIL( ); }

	virtual Coupling coupling( ) const               { rs_rto_chan_FAIL( ); }

	virtual Coupling set_coupling( Coupling )        { rs_rto_chan_FAIL( ); }

	virtual double scale( )                          { rs_rto_chan_FAIL( ); }

	virtual double set_scale( double )               { rs_rto_chan_FAIL( ); }

    virtual double min_scale( )                      { rs_rto_chan_FAIL( ); }

    virtual double max_scale( )                      { rs_rto_chan_FAIL( ); }

	virtual double offset( )                         { rs_rto_chan_FAIL( ); }

	virtual	double set_offset( double )              { rs_rto_chan_FAIL( ); }

    virtual double min_offset( )                     { rs_rto_chan_FAIL( ); }

    virtual double max_offset( )                     { rs_rto_chan_FAIL( ); }

	virtual double position( ) const                 { rs_rto_chan_FAIL( ); }

	virtual double set_position( double )            { rs_rto_chan_FAIL( ); }

	virtual double impedance( ) const                { rs_rto_chan_FAIL( ); }

	virtual double set_impedance( double )           { rs_rto_chan_FAIL( ); }

	virtual double min_impedance( ) const            { rs_rto_chan_FAIL( ); }

	virtual double max_impedance( ) const            { rs_rto_chan_FAIL( ); }

	virtual Bandwidth bandwidth( ) const             { rs_rto_chan_FAIL( ); }

	virtual Bandwidth set_bandwidth( Bandwidth )     { rs_rto_chan_FAIL( ); }

	virtual Filter_Type filter_type( ) const         { rs_rto_chan_FAIL( ); }

	virtual Filter_Type set_filter_type( Filter_Type )
                                                     { rs_rto_chan_FAIL( ); }

	virtual Filter_Cut_Off cut_off( ) const          { rs_rto_chan_FAIL( ); }
	virtual Filter_Cut_Off set_cut_off( Filter_Cut_Off )
                                                     { rs_rto_chan_FAIL( ); }

	virtual Data_Header header( )                    { rs_rto_chan_FAIL( ); }

	virtual std::vector< double > data( )            { rs_rto_chan_FAIL( ); }

	virtual std::vector< std::vector< double > >
    segment_data( )                                  { rs_rto_chan_FAIL( ); }

	virtual std::vector< std::vector< double > >
    segment_data( unsigned long )                    { rs_rto_chan_FAIL( ); }

	virtual std::vector< std::vector< double > >
    segment_data( unsigned long, unsigned long )     { rs_rto_chan_FAIL( ); }

    virtual std::string function( )                  { rs_rto_chan_FAIL( ); }

    virtual std::string
    set_function( std::string const & )              { rs_rto_chan_FAIL( ); }


  protected:

	rs_rto_chan( RS_RTO & rs,
				 Channel  ch );

	RS_RTO & m_rs;

	Channel m_channel;

	std::string m_name;


  private:

	rs_rto_chan( rs_rto_chan const & ) = delete;

	rs_rto_chan &
	operator = ( rs_rto_chan const & ) = delete;

	// Private methods that may be supported by the channels

    virtual Arith_Mode arith_mode( ) const           { rs_rto_chan_FAIL( ); }
    virtual Arith_Mode set_arith_mode( Arith_Mode )  { rs_rto_chan_FAIL( ); }

	std::string
	fail( std::string const & func ) const
	{
        std::string name = m_name.substr( m_name.rfind( ':' ) + 1 );

		if ( ! m_channel_exists )
			return "Channel '" + name + "' does not exist";

		return "Channel '" + name + "' has no " + func + "() method";
	}

	bool m_channel_exists = true;
};


}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
