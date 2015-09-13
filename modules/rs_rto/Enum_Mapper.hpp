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


/* Helper function for conversion between enum class types */


#pragma once
#if ! defined Enum_Mapper_hpp_
#define Enum_Mapper_hpp_


#include <map>
#include <type_traits>
#include <string>


/*----------------------------------------------------*
 * Class for bidirectional mapping between a C++ enumeration class.
 * and in principle arbitrary values (typically used to convert to
 * values from a corresponding C enumeration). Requires a map that
 * translates between the values and a short text snipped describing
 * what's mapped (for error messages).
 *----------------------------------------------------*/

template< typename Enum_Type, typename Value_Type  >
class Enum_Mapper
{
  public:

	Enum_Mapper( std::map< Value_Type, Enum_Type > const & mapper,
                 std::string                       const & name )
		: m_v2e_map( mapper )
		, m_name( name )
	{
        for ( auto const & p : m_v2e_map )
            m_e2v_map[ p.second ] = p.first;
    }

	Enum_Mapper( std::map< Enum_Type, Value_Type > const & mapper,
                 std::string                       const & name )
		: m_e2v_map( mapper )
		, m_name( name )
	{
        for ( auto const & p : m_e2v_map )
            m_v2e_map[ p.second ] = p.first;
    }

    // Transformation from C++ to C enuneration - can only fail if the
    // map is incomplete.

    Value_Type
	e2v( Enum_Type const & e ) const
	{
		auto it = m_e2v_map.find( e );
		if ( it == m_e2v_map.end( ) )
            throw std::runtime_error(   "Internal error, " + m_name
                                      + " map incomplete" );
		return it->second;
	}

    // Translation from C to C++ enumeration - can easily fail since
    // it can be called with arbitrary values.

	Enum_Type
	v2e( Value_Type  const & v,
         std::string const & name = "" ) const
	{
		auto it = m_v2e_map.find( v );
		if ( it == m_v2e_map.end( ) )
			throw std::invalid_argument(  "Invalid "
                                         + ( name.empty( ) ? m_name : name ) );
		return it->second;
	}


  private:

	std::map< Value_Type, Enum_Type > m_v2e_map;
    std::map< Enum_Type, Value_Type > m_e2v_map;
	std::string m_name;
};


// Conversion from an C++ enum class to the value of the underlying
// type is pretty simple...

template< typename T >
constexpr typename std::underlying_type< T >::type
enum_to_value( T val )
{
    return static_cast< typename std::underlying_type< T >::type >( val );
}


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

