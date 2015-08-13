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


#if ! defined VXI11_Client_hpp_
#define VXI11_Client_hpp_

#include <rpc/rpc.h>
#include <string>
#include <vector>
#include "vxi11.h"
#include "Client_Logger.hpp"


/*--------------------------------------------------*
 *--------------------------------------------------*/

class VXI11_Client : protected Client_Logger
{
  public:

    VXI11_Client( std::string const & device_name,
                  std::string const & address,
                  std::string const & vxi11_name = "inst0",
                  Log_Level           log_level = Log_Level::Normal );

    virtual
    ~VXI11_Client( )
    {
        disconnect( );
    }

    virtual
    bool
    connect( bool   create_async = false,
             bool   lock_device  = false,
             double lock_timeout = -1 );

    bool
    async_connect( );

    virtual
    bool
    disconnect( );

    bool
    async_disconnect( );

    // Tries to get exclusive access to device

    virtual
    bool
    lock( double lock_timeout = -1 );

    // Release exclusive access lock

    virtual
    bool
    unlock( );

    // Lock or unlock front panel

    virtual
    bool
    lock_out( bool   state,
              double timeout = -1 );

    bool
    read_stb( unsigned char & stb,
              double          timeout = -1 );

    virtual
    bool
    clear( double timeout = -1 );

    virtual
    bool
    trigger( double timeout = -1 );

    // Abort read or wriite via async channel

    virtual
    bool
    abort( );

    // Send complete buffer

    virtual
    bool
    write( std::string const & data,
           bool                without_eoi = false,
           double              timeout = -1 );

    // Read data up to 'max_len' bytes

    virtual
    bool
    read( std::string   & data,
          unsigned long   max_len = 0,
          double          timeout = -1 );

    // Read data until 'termchar' is encountered

    virtual
    bool
    read_eos( std::string & data,
              char          termchar,
              double        timeout = -1 );

    virtual
    bool
    read_eos( std::string & data )
    {
        return read_eos( data, m_default_term_char, -1 );
    }

    // Methods for retrieving various flags

    virtual
    bool
    is_connected( ) const
    {
        return m_core_client;
    }

    bool
    is_async_connected( ) const
    {
        return m_async_client;
    }

    virtual
    bool
    is_locked( ) const
    {
        return m_is_locked;
    }

    virtual
    bool
    more_data_available( ) const
    {
        return m_more_available;
    }

    virtual
    bool
    timed_out( ) const
    {
        return m_timed_out;
    }

    // Methods for setting and retrieving default timeouts

    double
    default_lock_timeout( ) const
    {
        return 0.001 * m_default_lock_timeout;
    }

    void
    set_default_lock_timeout( double timeout )
    {
        set_default_timeout( m_default_lock_timeout, timeout );
    }

    double
    default_read_timeout( ) const
    {
        return 0.001 * m_default_read_timeout;
    }

    void
    set_default_read_timeout( double timeout )
    {
        set_default_timeout( m_default_read_timeout, timeout );
    }

    double
    default_write_timeout( ) const
    {
        return 0.001 * m_default_write_timeout;
    }

    void
    set_default_write_timeout( double timeout )
    {
        set_default_timeout( m_default_write_timeout, timeout );
    }

    // Returns or sets the character that indicates end of message

    char
    default_termchar( ) const
    {
        return m_default_term_char;
    }

    void
    set_default_termchar( char t )
    {
        m_default_term_char = t;
    }


  private:

    /* Disable copy constructor and copy operator, we dan't want to 
       have suddenly two or more connections */

    VXI11_Client( VXI11_Client const & ) = delete;
    VXI11_Client & operator= ( VXI11_Client const & ) = delete;

    typedef enum
    {
        WAITLOCK_SET = ( 1 << 0 ),
        END_SET      = ( 1 << 3 ),
        TERMCHR_SET  = ( 1 << 7 )
    } Flags;

    typedef enum
    {
        REQCNT = ( 1 << 0 ),         // Transmission ended because as
                                     // many bytes as requested have
                                     // been received */
        CHR    = ( 1 << 1 ),         // Transmission ended because the
                                     // termchar' was received */
        END    = ( 1 << 2 ),         // Transmission ended because there
                                     // are no more bytes to receive */
    } Reason;

    char const *
    sperror( Device_ErrorCode err,
             int              rpc_res );

    unsigned long
    to_ms( double        timeout,
           unsigned long default_val );

    void
    update_timeout( unsigned long  & timeout,
                    struct timeval & before );

    bool
    timed_out( Device_ErrorCode    err,
               int                 rpc_res,
               std::string const & snippet );

    void
    set_default_timeout( unsigned long & which,
                         double          value );

    void
    set_rpc_timeout( unsigned long timeout_ms );

    // Strings for IP address and VXI-11 name of device

    std::string m_ip;
    std::string m_vxi11_name;

    // Client and link for core channel

    CLIENT          * m_core_client = nullptr;
    Create_LinkResp m_core_link;

    // Client and link for (optional) asynchronous channel

    CLIENT          * m_async_client = nullptr;
    Create_LinkResp m_async_link;

    // Flags

    bool m_more_available = false;
    bool m_timed_out = false;
    bool m_is_locked = false;

    // Default values for timeouts and termchar

    unsigned long m_default_lock_timeout  = 500;     // 500 ms
    unsigned long m_default_read_timeout  = 500;     // 500 ms
    unsigned long m_default_write_timeout = 500;     // 500 ms

    char m_default_term_char = '\n';

    static int const s_TIMEOUT_ERROR;
    static std::vector< std::string > const s_err_list;

    static int const s_RPC_TIMEOUT_ERROR;
    static std::vector< std::string > const s_rpc_err_list;
};


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
