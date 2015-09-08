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
 * Class for converion between a C enumeration and C++
 * enumeration class. Requires a map that translates
 * between the values.
 *----------------------------------------------------*/

template< typename T >
class Enum_Mapper
{
    typedef typename std::underlying_type< T >::type mapped_type;

  public:

	Enum_Mapper( std::map< mapped_type, T > const & mapper,
			std::string                const & name )
		: m_mapper( mapper )
		, m_name( name )
	{ }

    // Transformation from C++ to C enuneration - can only fail if the
    // map is incomplete.

    mapped_type
	e2v( T const & e ) const
	{
        for ( auto m : m_mapper )
            if ( m.second == e )
                return m.first;

        throw std::runtime_error(   "Internal error, " + m_name
                                  + " map incomplete" );
	}

    // Translation from C to C++ enumeration - can easily fail since
    // it can be called with arbitrary integers.

	T
	v2e( mapped_type const & v,
         std::string const & name = "" ) const
	{
		auto it = m_mapper.find( v );
		if ( it == m_mapper.end( ) )
			throw std::invalid_argument( " Invalid"
                                         + ( name.empty( ) ? m_name : name ) );
		return it->second;
	}


  private:

	std::map< mapped_type, T > m_mapper;
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

