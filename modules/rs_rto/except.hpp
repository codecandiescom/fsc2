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
#if ! defined except_hpp_
#define except_hpp_


#include <exception>
#include <stdexcept>
#include <string>


/*----------------------------------------------------*
 *----------------------------------------------------*/

class comm_failure : public std::exception
{
  public:

    comm_failure( std::string const & mess = "" )
        : m_mess( "Communication failure: " + mess + '\n' )
    { }

    char const *
    what( ) const noexcept
    {
        return m_mess.c_str( );
    }

  private:

    std::string m_mess;
};


/*----------------------------------------------------*
 *----------------------------------------------------*/

class bad_data : public std::exception
{
  public:

    bad_data( std::string const & mess = "" )
        : m_mess( "Bad data from device: " + mess + '\n' )
    { }

    char const *
    what( ) const noexcept
    {
        return m_mess.c_str( );
    }

  private:

    std::string m_mess;
};


/*----------------------------------------------------*
 *----------------------------------------------------*/

class operational_error : public std::exception
{
  public:

    operational_error( std::string const & mess = "" )
        : m_mess( "Operational error: " + mess + '\n' )
    { }

    char const *
    what( ) const noexcept
    {
        return m_mess.c_str( );
    }

  private:

    std::string m_mess;
};


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
