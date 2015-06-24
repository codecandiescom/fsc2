#if ! defined rs_smb100a_h_
#define rs_smb100a_h_


#include "fsc2_module.h"
#include "rs_smb100a.conf"


#if    ! defined B101 && ! defined B102   \
    && ! defined B103 && ! defined B106   \
    && ! defined B112 && ! defined B112L
#error "Hardware option of device not defined in configuration file"
#endif


enum ALC_State
{
	ALC_STATE_OFF,
	ALC_STATE_ON,
	ALC_STATE_AUTO
};

enum Power_Mode
{
	POWER_MODE_NORMAL,
	POWER_MODE_LOW_NOISE,
	POWER_MODE_LOW_DISTORTION
};

enum Off_Mode
{
	OFF_MODE_UNCHANGED,
	OFF_MODE_FATT
};

enum Coupling
{
	COUPLING_AC,
	COUPLING_DC
};

enum Source
{
	SOURCE_INT,
	SOURCE_EXT,
	SOURCE_INT_EXT
};

enum Impedance
{
	IMPEDANCE_LOW,
	IMPEDANCE_G50,
	IMPEDANCE_G600,
	IMPEDANCE_G1K,
	IMPEDANCE_G10K,
	IMPEDANCE_HIGH
};

enum Mod_Mode
{
	MOD_MODE_NORMAL,
	MOD_MODE_LOW_NOISE,
	MOD_MODE_HIGH_DEVIATION
};

enum Slope
{
	SLOPE_POSITIVE,
	SLOPE_NEGATIVE
};

enum Polarity
{
	POLARITY_NORMAL,
	POLARITY_INVERTED
};


typedef struct
{
	bool           state;
	bool           state_has_been_set;
	enum Impedance imp;
} Outp;

typedef struct
{
    double          pow;
	bool            pow_has_been_set;
	double          pow_resolution;
	enum ALC_State  alc_state;
	bool            alc_state_has_been_set;
    enum Power_Mode mode;
	bool            mode_has_been_set;
    enum Off_Mode   off_mode;
	bool            off_mode_has_been_set;
} Pow;

typedef struct
{
	double freq;
	bool   freq_has_been_set;

	double freq_resolution;
	double min_freq;
	double max_freq;
} Freq;

typedef struct
{
	bool          state;
	bool          state_has_been_set;
	double        depth;
	double        depth_resolution;
    enum Coupling ext_coup;
    enum Source   source;
} AM;

typedef struct
{
	bool          state;
	bool          state_has_been_set;
	double        dev;
	enum Coupling ext_coup;
	enum Source   source;
	enum Mod_Mode mode;
} FM;

typedef struct
{
	bool          state;
	bool          state_has_been_set;
	double        dev;
	enum Coupling ext_coup;
	enum Source   source;
	enum Mod_Mode mode;
} PM;

typedef struct
{
	double         freq;
    double         freq_resolution;
    double         min_freq;
    double         max_freq;
	double         volts;
    double         volt_resolution;
    double         max_volts;
	enum Impedance imp;
} LFO;

typedef struct
{
	enum Slope     slope;
	enum Impedance imp;
} Inp;

typedef struct
{
	char const * default_name;
	char       * name;
	bool         processing_list;
} List;

typedef struct
{
	bool           has_pulse_mod;
	bool           state;
	bool           state_has_been_set;
	enum Polarity  pol;
	enum Impedance imp;
} PulM;

typedef struct
{
	bool is_connected;

	Outp outp;
	Pow  pow;
	Freq freq;
	AM   am;
	FM   fm;
	PM   pm;
	LFO  lfo;
	Inp  inp;
	List list;
	PulM pulm;
} rs_smb100a_T;


extern rs_smb100a_T * rs;


// Hook functions

int rs_smb100a_init_hook( void );
int rs_smb100a_test_hook( void );
int rs_smb100a_end_of_test_hook( void );
int rs_smb100a_exp_hook( void );
int rs_smb100a_end_of_exp_hook( void );

// EDL functions

Var_T * synthesizer_name( Var_T * v  UNUSED_ARG );
Var_T * synthesizer_state( Var_T * v );
Var_T * synthesizer_frequency( Var_T * v );
Var_T * synthesizer_attenuation( Var_T * v );
Var_T * synthesizer_min_attenuation( Var_T * v );
Var_T * synthesizer_max_attenuation( Var_T * v );
Var_T * synthesizer_automatic_level_control( Var_T * v );
Var_T * synthesizer_output_impedance( Var_T * v );
Var_T * synthesizer_protection_tripped( Var_T * v );
Var_T * synthesizer_reset_protection( Var_T * v );
Var_T * synthesizer_rf_mode( Var_T * v );
Var_T * synthesizer_rf_off_mode( Var_T * v );


// Function of the OUTP sub-system

void outp_init( void );
bool outp_state( void );
bool outp_set_state( bool state );
enum Impedance outp_impedance( void );
bool outp_protection_is_tripped( void );
bool outp_reset_protection( void );

// Function of the POW sub-system

void pow_init( void );
double pow_power( void );
double pow_set_power( double pow );
double pow_min_power( void );
double pow_max_power( double freq );
enum ALC_State pow_alc_state( void );
enum ALC_State pow_set_alc_state( enum ALC_State state );
enum Power_Mode pow_mode( void );
enum Power_Mode pow_set_mode( enum Power_Mode mode );
enum Off_Mode pow_off_mode( void );
enum Off_Mode pow_set_off_mode( enum Off_Mode mode );
double pow_check_power( double pow,
						double freq );

// Function of the FREQ sub-system

void freq_init( void );
double freq_frequency( void );
double freq_set_frequency( double freq );
double freq_min_frequency( void );
double freq_max_frequency( void );
double freq_check_frequency( double freq );
bool freq_list_mode( bool on_off );

// Function of the AM sub-system

void am_init( void );
bool am_state( void );
bool am_set_state( bool state );
double am_depth( void );
double am_set_depth( double depth );
double am_sensitivity( void );
enum Coupling am_coupling( void );
enum Coupling am_set_coupling( enum Coupling coup );
enum Source am_source( void );
enum Source am_set_source( enum Source source );
double am_check_depth( double depth );

// Function of the FM sub-system

void fm_init( void );
bool fm_state( void );
bool fm_set_state( bool state );
double fm_deviation( void );
double fm_set_deviation( double dev );
double fm_sensitivity( void );
enum Coupling fm_coupling( void );
enum Coupling fm_set_coupling( enum Coupling coup );
enum Source fm_source( void );
enum Source fm_set_source( enum Source source );
enum Mod_Mode fm_mode( void );
enum Mod_Mode fm_set_mode( enum Mod_Mode mode );
double fm_max_deviation( double        freq,
						 enum Mod_Mode mode );
double fm_check_deviation( double        dev,
						   double        freq,
						   enum Mod_Mode mode );

// Function of the PM sub-system

void pm_init( void );
bool pm_state( void );
bool pm_set_state( bool state );
double pm_deviation( void );
double pm_set_deviation( double dev );
double pm_sensitivity( void );
enum Coupling pm_coupling( void );
enum Coupling pm_set_coupling( enum Coupling coup );
enum Source pm_source( void );
enum Source pm_set_source( enum Source source );
enum Mod_Mode pm_mode( void );
enum Mod_Mode pm_set_mode( enum Mod_Mode mode );
double pm_max_deviation( double        freq,
						 enum Mod_Mode mode );
double pm_check_deviation( double        dev,
						   double        freq,
						   enum Mod_Mode mode );

// Function of the LFO sub-system

void lfo_init( void );
double lfo_frequency( void );
double lfo_set_frequency( double freq );
double lfo_voltage( void );
double lfo_set_voltage( double volts );
enum Impedance lfo_impedance( void );
enum Impedance lfo_set_impedance( enum Impedance imp );

// Function of the INP sub-system

void inp_init( void );
enum Slope inp_slope( void );
enum Slope inp_set_slope( enum Slope slope );
enum Impedance inp_impedance( void );
enum Impedance inp_set_impedance( enum Impedance imp );


// Function of the LIST sub-system

void list_init( void );
void list_cleanup( void );
void list_select( char const * name );
void list_setup_A( double const * freqs,
				   double const * pows,
				   size_t         len,
				   char   const * name );
void list_setup_B( double const * freqs,
				   double         pows,
				   size_t         len,
				   char   const * name );
void list_setup_C( double const * freqs,
				   size_t         len,
				   char   const * name );
void list_start( void );
void list_stop( bool keep_rf_on );
int list_index( void );
void list_delete_list( char const * name );
void list_check_list_name( char const * name );

// Function of the PULM sub-system

void pulm_init( void );
bool pulm_state( void );
bool pulm_set_state( bool state );
enum Polarity pulm_polarity( void );
enum Polarity pulm_set_polarity( enum Polarity pol );
enum Impedance pulm_impedance( void );
enum Impedance pulm_set_impedance( enum Impedance imp );
void pulm_check_has_mod( void );
bool pulm_available( void );

// Utility functions

void rs_init( rs_smb100a_T * rs_cur );
void rs_cleanup( void );
void base_init( void );
void base_cleanup( void );
void rs_write( char const * data );
void rs_talk( char const * cmd,
			  char       * reply,
			  size_t       length );
bool query_bool( char const * cmd );
int query_int( char const * cmd );
double query_double( const char * cmd );
enum ALC_State query_alc_state( char const * cmd );
enum Coupling query_coupling( char const * cmd );
enum Source query_source( char const * cmd );
enum Power_Mode query_pow_mode( char const * cmd );
enum Off_Mode query_off_mode( char const * cmd );
enum Mod_Mode query_mod_mode( char const * cmd );
enum Impedance query_imp( char const * cmd );
enum Slope query_slope( char const * cmd );
enum Polarity query_pol( char const * cmd );
int bad_data( void );
long impedance_to_int( enum Impedance imp );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
