/*
 *  Copyright (C) 1999-2011 Jens Thoms Toerring
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


#if ! defined RUN_HEADER
#define RUN_HEADER


#include "fsc2.h"


/* Settings for GPIB logging */

#if ! defined ( GPIB_LOG_FILE )
#define GPIB_LOG_FILE NULL
#endif

#if ! defined ( GPIB_LOG_LEVEL )
#define GPIB_LOG_LEVEL LL_ERR
#endif

/* Signals sent by the parent/child and accepted by the other side */

#define DO_QUIT   SIGUSR2
#define QUITTING  DO_QUIT


bool run( void );

void run_stop_button_callback( FL_OBJECT * /* a */,
                               long        /* b */  );

void run_close_button_callback( FL_OBJECT * /* a */,
                                long        /* b */  );


#endif  /* ! RUN_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
