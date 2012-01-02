/*
 *  Copyright (C) 1999-2012 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if ! defined LOCKS_HEADER
#define LOCKS_HEADER


bool fsc2_obtain_uucp_lock( const char * /* name */ );

void fsc2_release_uucp_lock( const char * /* name */ );

bool fsc2_obtain_fcntl_lock( FILE * /* fp */,
                             int    /* lock_type */,
                             bool   /* wait_for_lock */ );

void fsc2_release_fcntl_lock( FILE * /* fp */ );

#endif  /* ! LOCKS_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
