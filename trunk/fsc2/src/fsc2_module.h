/*
  $Id$

  Copyright (C) 1999-2003 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#if ! defined FSC2_MODULE_HEADER
#define FSC2_MODULE_HEADER


#define _GNU_SOURCE 1


/* Inclusion of system header files */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <forms.h>
#include <sys/types.h>
#include <sys/stat.h>


/* inclusion of programs own header files as far as necessary */

#include "fsc2_config.h"
#include "fsc2_types.h"
#include "global.h"
#include "fsc2_assert.h"
#include "inline.h"
#include "exceptions.h"
#include "T.h"
#include "variables.h"
#include "util.h"
#include "func.h"
#include "func_basic.h"
#include "func_util.h"
#include "func_save.h"
#include "loader.h"
#include "phases.h"
#include "pulser.h"
#include "serial.h"
#include "gpib_if.h"
#include "module_util.h"


#if defined MAPATROL
#include <mpatrol.h>
#endif


#define FSC2_MODE get_mode( )
#define FSC2_IS_CHECK_RUN get_check_state( )


extern void show_message( const char *str );
extern void show_alert( const char *str );
extern int show_choices( const char *text, int numb, const char *b1,
						 const char *b2, const char *b3, int def );
extern const char *show_input( const char *content, const char *label );


/* Global variables */

extern Pulser_Struct *pulser_struct;
extern Phase_Sequence *PSeq;
extern Acquisition_Sequence ASeq[ ];
extern long Cur_Pulser;

extern bool need_GPIB;


extern const char *Channel_Names[ NUM_CHANNEL_NAMES ];
extern const char *Function_Names[ PULSER_CHANNEL_NUM_FUNC ];
extern const char *Phase_Types[ NUM_PHASE_TYPES ];


/* The following must be defined after the declaration of pulser_struct ! */

#define pulser_struct pulser_struct[ Cur_Pulser ]


#endif  /* ! FSC2_MODULE_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
