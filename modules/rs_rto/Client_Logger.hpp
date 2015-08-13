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


#if ! defined Client_Logger_hpp_
#define Client_Logger_hpp_


#include <iostream>
#include <cstdio>
#include <string>


/*--------------------------------------------------*
 *--------------------------------------------------*/

enum class Log_Level
{
    None,              // no logging at all (and no file opened)
    Low,               // logs errors only
    Normal,            // logs function calls and errors
    High               // logs function calls, all data and errors
};


/*--------------------------------------------------*
 *--------------------------------------------------*/

class Client_Logger
{
  public:

    Client_Logger( std::string const & device_name,
                   Log_Level           level = Log_Level::Normal )
        : m_device_name( device_name )
        , m_level( level )
    { }

    virtual
    ~Client_Logger( );

    void
    set_log_level( Log_Level level )
    {
        m_level = level;
    }

    bool
    set_log_filename( std::string const & file_name,
                      std::string const & dir = "",
                      bool                delete_old_file = false );

    bool
    set_log_file( FILE * fp,
                  bool   delete_old_file = false );

    bool
    set_log_descriptor( int  fd,
                        bool delete_old_file = false );

    void
    log_function_start( std::string const & function_name );

    void
    log_function_end( std::string const & function_name );

    void
    log_data( const char * buffer,
              long         length );

    void
    log_data( std::string const & buffer,
              long                max_length = -1 );

    void
    log_error( char const * format,
               ... );

    void
    log_message( char const * format,
                 ... );
    bool
    delete_log( );


  protected:

    FILE *
    log_file( )
    {
        return m_fp;
    }


    std::string
    last_error(  ) const
    {
        return m_last_error;
    }


  protected:

    std::string m_device_name;


  private:

    bool
    open_file( );

    void
    log_date_and_name( );

    std::string
    default_name( );


    Log_Level m_level;

    FILE * m_fp = NULL;

    std::string m_name;

    bool m_is_our_file = false;     // set if logger opend the file

    std::string m_last_error;

    struct timeval m_tv;

    struct timeval m_now;
};


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
