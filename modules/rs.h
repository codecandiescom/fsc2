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

enum Mod_Type
{
	MOD_TYPE_AM,
	MOD_TYPE_FM,
	MOD_TYPE_PM,
	MOD_TYPE_PULM
};

enum Mod_Source
{
	MOD_SOURCE_EXT_AC,
	MOD_SOURCE_EXT_DC,
	MOD_SOURCE_INT
};

typedef struct
{
	bool           state;
	bool           state_has_been_set;
	enum Impedance imp;
} Outp_T;

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
} Pow_T;

typedef struct
{
	double freq;
	bool   freq_has_been_set;

	double freq_resolution;
	double min_freq;
	double max_freq;
} Freq_T;

typedef struct
{
	bool          state;
	bool          state_has_been_set;
	double        depth;
	bool          depth_has_been_set;
	double        depth_resolution;
    enum Coupling ext_coup;
	bool          ext_coup_has_been_set;
    enum Source   source;
	bool          source_has_been_set;
} AM_T;

typedef struct
{
	bool          state;
	double        dev;
	bool          dev_has_been_set;
	enum Coupling ext_coup;
	bool          ext_coup_has_been_set;
	enum Source   source;
	bool          source_has_been_set;
	enum Mod_Mode mode;
	bool          mode_has_been_set;
} FM_T;

typedef struct
{
	bool          state;
	double        dev;
	bool          dev_has_been_set;
	enum Coupling ext_coup;
	bool          ext_coup_has_been_set;
	enum Source   source;
	bool          source_has_been_set;
	enum Mod_Mode mode;
	bool          mode_has_been_set;
} PM_T;

typedef struct
{
	double         freq;
	bool           freq_has_been_set;
    double         freq_resolution;
    double         min_freq;
    double         max_freq;
	double         volts;
	bool           volts_has_been_set;
    double         volt_resolution;
    double         max_volts;
	enum Impedance imp;
	bool           imp_has_been_set;
} LFO_T;

typedef struct
{
	enum Slope     slope;
	enum Impedance imp;
} Inp_T;

typedef struct
{
	char const * default_name;
	char       * name;
	bool         processing_list;
} List_T;

typedef struct
{
	bool           has_pulse_mod;
	bool           state;
	enum Polarity  pol;
	bool           pol_has_been_set;
	enum Impedance imp;
	bool           imp_has_been_set;
} PulM_T;

typedef struct
{
	bool           ( * get_state )( void );
	bool           ( * set_state )( bool );
	double         ( * get_amplitude )( void );
	double         ( * set_amplitude )( double );
	enum Coupling  ( * get_coupling )( void );
	enum Coupling  ( * set_coupling )( enum Coupling );
	enum Source    ( * get_source )( void );
	enum Source    ( * set_source )( enum Source );
	enum Mod_Mode  ( * get_mode )( void );
	enum Mod_Mode  ( * set_mode )( enum Mod_Mode );
	enum Impedance ( * get_impedance )( void );
	enum Impedance ( * set_impedance )( enum Impedance );
	enum Polarity  ( * get_polarity )( void );
	enum Polarity  ( * set_polarity )( enum Polarity );
} Mod_Funcs;

typedef struct
{
	enum Mod_Type   type;
	bool            type_has_been_set;
	bool            state[ 4 ];
	bool            state_has_been_set[ 4 ];
	Mod_Funcs     * funcs;
} Mod_T;

typedef struct
{
	bool is_connected;

	Outp_T outp;
	Pow_T  pow;
	Freq_T freq;
	AM_T   am;
	FM_T   fm;
	PM_T   pm;
	LFO_T  lfo;
	Inp_T  inp;
	List_T list;
	PulM_T pulm;
	Mod_T  mod;
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
Var_T * synthesizer_mod_freq( Var_T * v );
Var_T * synthesizer_mod_output_impedance( Var_T * v );
Var_T * synthesizer_mod_output_voltage( Var_T * v );
Var_T * synthesizer_mod_type( Var_T * v );
Var_T * synthesizer_mod_state( Var_T * v );
Var_T * synthesizer_mod_amp( Var_T * v );
Var_T * synthesizer_mod_source( Var_T * v );


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
enum Impedance int_to_impedance( long imp );
char const * impedance_to_name( enum Impedance imp );


// Functions for dealing with modulation

void mod_init( void );
enum Mod_Type mod_type( void );
enum Mod_Type mod_set_type( enum Mod_Type type );
bool mod_state( void );
bool mod_set_state( bool state );
double mod_amplitude( void );
double mod_set_amplitude( double amp );
enum Coupling mod_coupling( void );
enum Coupling mod_set_coupling( enum Coupling coup );
enum Source mod_source( void );
enum Source mod_set_source( enum Source source );
enum Mod_Mode mod_mode( void );
enum Mod_Mode mod_set_mode( enum Mod_Mode mode );
enum Impedance mod_impedance( void );
enum Impedance mod_set_impedance( enum Impedance imp );
enum Polarity mod_polarity( void );
enum Polarity mod_set_polarity( enum Polarity pol );


#endif


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
