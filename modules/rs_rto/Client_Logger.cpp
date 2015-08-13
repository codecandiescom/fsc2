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


#include "Client_Logger.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <time.h>
#include <sys/timeb.h>
#include <cerrno>
#include <unistd.h>


/*--------------------------------------------------*
 * On destruction close the log file
 *--------------------------------------------------*/

Client_Logger::~Client_Logger( )
{
    if ( m_fp )
    {
        log_message( "Stopping logging" );
        if ( m_is_our_file )
            fclose( m_fp );
        m_fp = nullptr;
    }
}


/*--------------------------------------------------*
 * Writes a message about entry into a function.
 * Log level must be at least 'Normal'.
 *--------------------------------------------------*/

void
Client_Logger::log_function_start( std::string const & function_name )
{
    if ( m_level < Log_Level::Normal || ! open_file( ) )
        return;

    log_date_and_name( );
    fprintf( m_fp, "Call of %s()\n", function_name.c_str( ) );
    fflush( m_fp );
}


/*--------------------------------------------------*
 * Writes a message about exit from a function
 * Log level must be at least 'Normal'.
 *--------------------------------------------------*/

void
Client_Logger::log_function_end( std::string const & function_name )
{
    if ( m_level < Log_Level::Normal || ! open_file( ) )
        return;

    log_date_and_name( );
    fprintf( m_fp, "Exit of %s()\n", function_name.c_str( ) );
    fflush( m_fp );
}


/*------------------------------------------------*
 * Writes a whole set of data (with a trailing newine)
 * from a char buffer. Log level must be 'High'.
 *------------------------------------------------*/

void
Client_Logger::log_data( const char * buffer,
                         long         length )
{
    if ( m_level < Log_Level::High || length <= 0 || ! open_file( ) )
        return;

    while ( length-- > 0 )
        fputc( *buffer++, m_fp );
    fputc( '\n', m_fp );
    fflush( m_fp );
}


/*------------------------------------------------*
 * Writes a whole set of data (with a trailing newine)
 * from a std::string ('max_length' is optional, default
 * is to write everything). Log level must be 'High'.
 *------------------------------------------------*/

void
Client_Logger::log_data( std::string const & buffer,
                         long                max_length )
{
    if ( m_level < Log_Level::High || ! open_file( ) )
        return;

    if ( max_length < 0 || max_length > static_cast< long >( buffer.size( ) ) )
        max_length = buffer.size( );

    char const * b = buffer.c_str( );

    while ( max_length-- > 0 )
        fputc( *b++, m_fp );
    fputc( '\n', m_fp );
    fflush( m_fp );
}


/*------------------------------------------------*
 * Writes out an  message, log level must be at least 'Low'.
 * Also stores the message (even if no logging happens) in
 * a string the user can request. A line-feed is appended
 * autinatically to the log file.
 *------------------------------------------------*/

void
Client_Logger::log_error( char const * fmt,
                          ... )
{
    if ( ! fmt || ! *fmt )
        return;

    va_list ap1, ap2;
        
    va_start( ap1, fmt );
    va_copy( ap2, ap1 );
    int cnt = vsnprintf( nullptr, 0, fmt, ap1 );
    va_end( ap1 );

    char buf[ cnt + 2 ];
    vsprintf( buf, fmt, ap2 );
    va_end( ap2 );

    m_last_error = buf;

    if ( m_level < Log_Level::Low || ! open_file( ) )
        return;

    log_date_and_name( );
    strcat( buf, "\n" );
    fwrite( buf, 1, cnt + 1, m_fp );
    fflush( m_fp );
}


/*------------------------------------------------*
 * Writes out a normal message, log level must be at least
 * 'Normal'. A line-feed is appended autinatically.
 *------------------------------------------------*/

void
Client_Logger::log_message( char const * fmt,
                            ... )
{
    if ( m_level < Log_Level::Normal || ! open_file( ) )
        return;

    log_date_and_name( );

    va_list ap;
    va_start( ap, fmt );
    vfprintf( m_fp, fmt, ap );
    va_end( ap );
    fprintf( m_fp, "\n" );
    fflush( m_fp );
}


/*--------------------------------------------------*
 * Requests to open a new file for logging. By setting
 * 'delete_old_file' the old one gets deleted if it
 * was opened by us.
 *--------------------------------------------------*/

bool
Client_Logger::set_log_filename( std::string const & file_name,
                                 std::string const & dir,
                                 bool                delete_old_file )
{
    if (    m_level == Log_Level::None
         || ( ! dir.empty( ) && file_name[ 0 ] == '/' )
         || file_name.empty( ) )
    {
        errno = EINVAL;
        return false;
    }

    std::string path;
    if ( ! dir.empty( ) )
        path = dir + "/" + file_name;
    else
        path = file_name;

    errno = 0;
    FILE * new_fp = fopen( path.c_str( ), "w" );

    if ( ! new_fp )
        return false;

    if ( m_fp )
    {
        log_message( "Stopping logging" );

        if ( m_is_our_file )
        {
            fclose( m_fp );
            if ( delete_old_file )
                ::unlink( m_name.c_str( ) );
        }
    }

    m_fp = new_fp;
    m_name = path;
    m_is_our_file = true;
    log_message( "Starting logging" );
    return true;
}


/*--------------------------------------------------*
 * Requests to switch logging to an (open) FILE pointer.
 * By setting 'delete_old_file' the old one gets deleted
 * if it was opened by us.
 *--------------------------------------------------*/

bool
Client_Logger::set_log_file( FILE * fp,
                             bool   delete_old_file )
{
    if ( m_level == Log_Level::None )
        return true;

    if ( ! fp )
        return false;

    if ( m_fp )
    {
        log_message( "Stopping logging" );
        if ( m_is_our_file )
        {
            fclose( m_fp );
            if ( delete_old_file )
                ::unlink( m_name.c_str( ) );
        }
    }

    m_fp = fp;
    m_name.clear( );
    m_is_our_file = false;
    log_message( "Starting logging\n" );

    return true;
}


/*--------------------------------------------------*
 * Requests to switch logging to an (open) file descrptor.
 * By setting 'delete_old_file' the old one gets deleted
 * if it was opened by us.
 *--------------------------------------------------*/

bool
Client_Logger::set_log_descriptor( int  fd,
                                   bool delete_old_file )
{
    if ( m_level == Log_Level::None )
        return true;

    if ( fd < 0 )
        return false;

    if ( m_fp )
    {
        log_message( "Stopping logging" );
        if ( m_is_our_file )
        {
            fclose( m_fp );
            if ( delete_old_file )
                ::unlink( m_name.c_str( ) );
        }
    }

    m_fp = fdopen( fd, "w" );
    m_name.clear( );
    m_is_our_file = false;
    if ( m_fp )
        log_message( "Starting logging\n" );

    return m_fp;
}


/*--------------------------------------------------*
 * Requests that the currently used log file gets closed
 * and deleted. Can only be done wen it was us that created
 * it.
 *--------------------------------------------------*/

bool
Client_Logger::delete_log( )
{
    if ( ! m_is_our_file )
        return false;

    if ( m_fp )
    {
        fclose( m_fp );
        unlink( m_name.c_str( ) );
    }

    m_fp = NULL;
    m_name.clear( );

    return true;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

bool
Client_Logger::open_file( )
{
    if ( m_level == Log_Level::None || ( ! m_fp && m_is_our_file ) )
        return false;

    if ( m_fp )
        return true;

    if ( m_device_name.empty( ) )
    {
        m_is_our_file = true;
        return false;
    }

    const char * dir = getenv( "TMP" );
    if ( ! dir )
        dir = "/tmp";

    m_name = dir + ( "/" + m_device_name ) + ".log";
    m_is_our_file = true;

    if ( ( m_fp = fopen( m_name.c_str( ), "w" ) ) )
        log_message( "Starting logging\n" );

    return m_fp;
}


/*--------------------------------------------------*
 *--------------------------------------------------*/

void
Client_Logger::log_date_and_name( )
{
    time_t t = time( nullptr );

    char tc[ 26 ];
    strcpy( tc, ctime( &t ) );
    tc[ 10 ] = '\0';
    tc[ 19 ] = '\0';
    tc[ 24 ] = '\0';

    struct timeb mt;
    ftime( &mt );

    fprintf( m_fp, "[%s %s %s.%03d] %s: ", tc + 4, tc + 20, tc + 11,
             mt.millitm, m_device_name.c_str( ) );
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
