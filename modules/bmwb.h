#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

#include "fsc2_config.h"
#include "bmwb_rsc.h"


#if defined __GNUC__
#define UNUSED_ARG __attribute__ ((unused))
#else
#define UNUSED_ARG
#endif


#define BMWB_X_BAND_STATE_FILE    "bmwb_x_band.state"
#define BMWB_Q_BAND_STATE_FILE    "bmwb_q_band.state"

#define X_BAND            0
#define Q_BAND            1

#define MODE_STANDBY      0
#define MODE_TUNE         1
#define MODE_OPERATE      2

#define MIN_ATTENUATION   0
#define MAX_ATTENUATION   60

#define X_BAND_MIN_FREQ   9.3
#define X_BAND_MAX_FREQ   9.6

#define Q_BAND_MIN_FREQ  34.0
#define Q_BAND_MAX_FREQ  36.0

#define UNCALIBRATED_MIN -0.5
#define UNCALIBRATED_MAX  2.5


#define FAC              356.97577




typedef struct {
    uid_t         EUID;                  /* user and group ID the program got */
    gid_t         EGID;                  /* started with */
	int           type;
	int           mode;
	double        freq;
	double        min_freq;
	double        max_freq;
	int           attenuation;
	double        signal_phase;
	double        bias;
	double        lock_phase;
	int           unlocked_state;
	double        uncalibrated;
	int           afc_state;
	FD_bmbw_rsc * rsc;
} BMWB;

extern BMWB bmwb;

int meilhaus_init( void );
void meilhaus_finish( void );
void raise_permissions( void );
void lower_permissions( void );
FILE * bmwb_fopen( const char *,
				   const char * );
double d_max( double  a,
			  double  b );
double d_min( double  a,
			  double  b );
int irnd( double d );
char * get_string( const char * fmt,
				   ... );
const char * slash( const char * path );
