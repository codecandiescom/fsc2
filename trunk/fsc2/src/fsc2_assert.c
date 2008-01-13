/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
 * 
 *  This file is part of fsc2.
 * 
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 * 
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2.h"
#include <execinfo.h>


Fsc2_Assert_T Assert_Struct;


int
fsc2_assert_print( const char * expression,
                   const char * filename,
                   int          line )
{
    fprintf( stderr, "%s:%d: failed assertion: %s\n", filename, line,
             expression );
    Assert_Struct.expression = expression;
    Assert_Struct.line = line;
    Assert_Struct.filename = filename;

    Crash.trace_length = backtrace( Crash.trace, MAX_TRACE_LEN );
    memmove( Crash.trace, Crash.trace + 1, --Crash.trace_length );
    abort( );

    return 0;
}


int
fsc2_impossible_print( const char * filename,
                       int          line )
{
    fprintf( stderr, "%s:%d: failed assertion: impossible situation "
             "encountered.\n", filename, line );
    Assert_Struct.expression = "Impossible situation encountered.";
    Assert_Struct.line = line;
    Assert_Struct.filename = filename;
    Crash.trace_length = backtrace( Crash.trace, MAX_TRACE_LEN );
    memmove( Crash.trace, Crash.trace + 1, --Crash.trace_length );
    abort( );

    return 0;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
