/*
  $Id$
*/


#if ! defined CONN_HEADER
#define CONN_HEADER

#include "fsc2.h"

#define BUSY_SIGNAL    SIGUSR1
#define UNBUSY_SIGNAL  SIGUSR2


pid_t spawn_conn( bool start_state );


#endif  /* ! CONN_HEADER */
