/*
  $Id$
*/

#if ! defined RUN_HEADER
#define RUN_HEADER


#include "fsc2.h"

/* Signals sent by the parent and accepted by the child */

#define DO_SEND   SIGUSR1
#define DO_QUIT   SIGUSR2

/* Signals sent by the child and accepted by the parent */

#define NEW_DATA  SIGUSR1
#define QUITTING  SIGUSR2



bool run( void );


#endif  /* ! RUN_HEADER */
