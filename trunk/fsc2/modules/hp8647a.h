/*
 $Id$
*/


#include "fsc2.h"

#define DEVICE_NAME "HP8647A"      /* compare entry in /etc/gpib.conf ! */

#define MIN_FREQ        2.5e5            /* 250 kHz  */
#define MAX_FREQ        1.0e9            /* 1000 MHz */
#define MIN_ATTEN       10.0             /* +10 db   */
#define MAX_ATTEN      -136.0            /* -136 db  */
#define ATT_RESOLUTION  0.1

#define DEFAULT_TABLE_FILE "hp8647a.table"     /* libdir will be prepended ! */

#define DEF_ATT_REF_FREQ   1.4e7               /* 14 MHz */

/* declaration of exported functions */

int hp8647a_init_hook( void );
int hp8647a_test_hook( void );
int hp8647a_end_of_test_hook( void );
int hp8647a_exp_hook( void );
int hp8647a_end_of_exp_hook( void );
void hp8647a_exit_hook( void );


Var *synthesizer_state( Var *v );
Var *synthesizer_frequency( Var *v );
Var *synthesizer_step_frequency( Var *v );
Var *synthesizer_attenuation( Var *v );
Var *synthesizer_sweep_up( Var *v );
Var *synthesizer_sweep_down( Var *v );
Var *synthesizer_reset_frequency( Var *v );
Var *synthesizer_use_table( Var *v );
Var *synthesizer_attenuation( Var *v );
Var *synthesizer_att_ref_freq( Var *v );


typedef struct {
	double freq;
	double att;
} ATT_TABLE_ENTRY;


typedef struct
{
	int device;

	double freq;
	bool freq_is_set;
	double step_freq;
	bool step_freq_is_set;
	double start_freq;
	bool start_freq_is_set;
	double attenuation;
	bool attenuation_is_set;

	bool state;

	char *table_file;      /* name of attenuation table file */
	bool use_table;
	ATT_TABLE_ENTRY *att_table;
	long att_table_len;
	double min_table_freq;
	double max_table_freq;
	double att_ref_freq;
	double att_at_ref_freq;

} HP8647A;



#if defined( HP8647A_MAIN )

HP8647A hp8647a;

#else

extern HP8647A hp8647a;

#endif



/* functions defined in "hp8647a_util.c" */

void hp8647a_read_table( FILE *fp );
FILE *hp8647a_find_table( char *name );
FILE *hp8647a_open_table( char *name );
double hp8647a_get_att_from_table( double freq );


/* functions defined in "hp8647a_lexer.flex" */

void hp8647a_read_table( FILE *fp );
