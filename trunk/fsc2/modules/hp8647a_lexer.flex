/*
  $Id$
*/


		/*---------------------*/
		/*     DEFINITIONS     */
		/*---------------------*/

%option noyywrap stack nounput never-interactive

%{

#include "fsc2.h"
#include "hp8647a.h"

#define IS_FREQ 1
#define IS_ATT  2
#define TABLE_INIT -1
#define TABLE_END   0
#define TABLE_CHUNK_SIZE 128

static int hp8647a_lex( void );
static int hp8647a_get_unit( void );
static void hp8647a_new_table_entry( int type, double val );
static int hp8647a_comp( const void *a, const void *b );

static long hp8647a_Lc,
			hp8647a_Comm_Lc;
static double unit_fac = 1.0;

static long cur_table_len;

%}

REM1     ^[\t ]*("//"|"#").*\n
REM2     [\t ]*("//"|"#").*\n
REM3     "/*"
REM4     [^*\n]*
REM5     "*"+[^*/\n]*
EREM1    "*"+"/"
EREM2    "*"+"/"[\t ]*\n
WLWS     ^[\t ,;:]*\n
WS       [\t ,;:]+
TWS      [\t ,;:]+\n

INT      [0-9]+
EXPO     [EDed][+-]?{INT}
FLOAT    ((([0-9]+"."[0-9]*)|([0-9]*"."[0-9]+)){EXPO}?)|({INT}{EXPO})
D        {INT}|{FLOAT}
IU       {INT}[\t ]*((("k"|"M"|"G")*"Hz")|"dB")
FU       {FLOAT}[\t ]*((("k"|"M"|"G")*"Hz")|"dB")
DU       {IU}|{FU}


%x      comm


		/*---------------*/
%%		/*     RULES     */
		/*---------------*/


            /* handling of C++ style comment spanning a whole line */
{REM1}		hp8647a_Lc++;

			/* handling of C++ style comment not spanning a whole line */
{REM2}		hp8647a_Lc++;

			/* handling of C style comment */
{REM3}		{
				hp8647a_Comm_Lc = hp8647a_Lc;
				yy_push_state( comm );
			}

<comm>{

{REM4}		/* skipping anything that's not a '*' */
{REM5}      /* skipping all '*'s not followed by '/'s */

			/* end of line character in a comment */
\n			hp8647a_Lc++;

			/* handling of EOF within a comment -> fatal error */
<<EOF>>		THROW( EOF_IN_COMMENT_EXCEPTION );

			/* end of comment but not end of line */
{EREM1}		yy_pop_state( );

			/* end of comment and end of line */
{EREM2}     {
				hp8647a_Lc++;
				yy_pop_state( );
			}
} /* end of <comm> */

{EREM1}     THROW( DANGLING_END_OF_COMMENT )

			/* dump empty line (i.e. just containing tabs and spaces) */
{WLWS}      hp8647a_Lc++;

			/* dumps trailing white space */
{TWS}       hp8647a_Lc++;

			/* dump all other forms of white */
{WS}        

			/* count lines */
\n			hp8647a_Lc++;

{DU}        return hp8647a_get_unit( );

{D}         {
				unit_fac = 1.0;
				return IS_FREQ | IS_ATT;
			}

<<EOF>>		return 0;

.           THROW( INVALID_INPUT_EXCEPTION );


		/*----------------------*/
%%		/*     END OF RULES     */
		/*----------------------*/


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

void hp8647a_read_table( FILE *fp )
{
	static bool is_restart = UNSET;
	int type;


	if ( ( hp8647a_in = fp ) == NULL )
	{
		eprint( FATAL, "%s: Internal error while reading attenuation table.\n",
				DEVICE_NAME );
		THROW( EXCEPTION );
	}

	/* Keep the lexer happy... */

	if ( is_restart )
	    hp8647a_restart( hp8647a_in );
	else
		 is_restart = SET;

	hp8647a_Lc = 1;

	TRY {
		hp8647a_new_table_entry( TABLE_INIT, 0.0 );
		while( ( type = hp8647a_lex( ) ) != 0 )
			hp8647a_new_table_entry( type, T_atof( hp8647a_text ) * unit_fac );
		hp8647a_new_table_entry( TABLE_END, 0.0 );
		TRY_SUCCESS;
	}
	CATCH( EOF_IN_COMMENT_EXCEPTION )
	{
		eprint( FATAL, "%s: Unexpected end of table file `%s' in comment "
				"starting at line %ld.\n", DEVICE_NAME, hp8647a.table_file,
				hp8647a_Comm_Lc );
		THROW( EXCEPTION );
	}
	CATCH( DANGLING_END_OF_COMMENT )
	{
		eprint( FATAL, "%s: End of comment found in table file `%s' at line "
				"%ld.\n", DEVICE_NAME, hp8647a.table_file, hp8647a_Lc );
		THROW( EXCEPTION );
	}
	CATCH( INVALID_INPUT_EXCEPTION )
	{
		eprint( FATAL, "%s: Invalid entry in table file  `%s' at line %ld.\n",
				DEVICE_NAME, hp8647a.table_file, hp8647a_Lc );
		THROW( EXCEPTION );
	}
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static int hp8647a_get_unit( void )
{
	char *cp;
	bool done = UNSET;
	int type = 0;


	for ( cp = hp8647a_text; ! done && *cp; cp++ )
	{
		switch ( *cp )
		{
			 case ' ' : case '\t' :
				 *cp = '\0';
				 break;

			 case 'k' :
				  *cp = '\0';
				  unit_fac = 1.0e3;
				  type = IS_FREQ;
				  done = SET;
				  break;

			 case 'M' :
				  *cp = '\0';
				  type = IS_FREQ;
				  unit_fac = 1.0e6;
				  done = SET;
				  break;

			 case 'G' :
				  *cp = '\0';
				  type = IS_FREQ;
				  unit_fac = 1.0e9;
				  done = SET;
				  break;

			 case 'd' :
				  *cp = '\0';
				  type = IS_ATT;
				  unit_fac = 1.0;
				  done = SET;
				  break;
		}
	}

	return type;
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static void hp8647a_new_table_entry( int type, double val )
{
	static bool is_freq = UNSET, is_att = UNSET;


	/* Initialize everything when type is -1 */

	if ( type == TABLE_INIT )
	{
		is_freq = is_att = UNSET;
		hp8647a.att_table = T_malloc( TABLE_CHUNK_SIZE *
									  sizeof( ATT_TABLE_ENTRY ) );
	    cur_table_len = TABLE_CHUNK_SIZE;
		return;
	}

	/* If type is 0 this is the final check after all data have been read */

	if ( type == TABLE_END )
	{
		/* Check that we're not expecting another attenuation or frequency
		   while the end of the file is already reached */

		if ( is_freq ^ is_att )                 // one of them is still set ?
		{
			eprint( FATAL, "%s: Missing final %s in table file `%s'.\n",
					DEVICE_NAME, is_freq ? "attenuation" : "frequency",
					hp8647a.table_file );
			THROW( EXCEPTION );
		}

		/* Check that there are at least two entries in the table */

		if ( hp8647a.att_table_len < 2 )
		{
			eprint( FATAL, "%s: Table file `%s' contains less than 2 "
					"entries.\n", DEVICE_NAME, hp8647a.table_file );
			THROW( EXCEPTION );
		}

		/* Cut back table size to the amount of memory really needed and sort
		   the table in ascending order with respect to the frequencies,
		   finally get lowest and highest frequency */

		T_realloc( hp8647a.att_table,
				   hp8647a.att_table_len * sizeof ( ATT_TABLE_ENTRY ) );
		qsort( hp8647a.att_table, hp8647a.att_table_len,
			    sizeof ( ATT_TABLE_ENTRY ), hp8647a_comp );
		hp8647a.min_table_freq = hp8647a.att_table[ 0 ].freq;
		hp8647a.max_table_freq =
						   hp8647a.att_table[ hp8647a.att_table_len - 1 ].freq;
		return;
	}

	/* Two frequencies or two attenuations in a row aren't allowed (at least
	   when the first entry wasn't used up) */

	if ( type == IS_ATT && is_att )
	{
		eprint( FATAL, "%s: Error while reading table file `%s': 2 "
				"attenuation values in a row at line %ld.\n", DEVICE_NAME,	 
				hp8647a.table_file, hp8647a_Lc );
		THROW( EXCEPTION );
	}

	if ( type == IS_FREQ && is_freq )
	{
		eprint( FATAL, "%s: Error while reading table file `%s': 2 "
				"frequency values in a row at line %ld.\n", DEVICE_NAME,	 
				hp8647a.table_file, hp8647a_Lc );
		THROW( EXCEPTION );
	}

	/* If the type is indetermined either set it to the not yet set type
	   or to frequency if none has been set yet - thus the default, when no
	   units are given is frequency first, then the attenuation */

	if ( type == ( IS_FREQ | IS_ATT ) )
	{
		if ( is_freq )
		   type = IS_ATT;
		else
			type = IS_FREQ;
	}

	/* If necessary extend the length of the table */

	if ( cur_table_len <= hp8647a.att_table_len )
	{
		cur_table_len += TABLE_CHUNK_SIZE;
	    hp8647a.att_table = T_realloc( hp8647a.att_table, cur_table_len
									   * sizeof( ATT_TABLE_ENTRY ) );
	}

	if ( type == IS_FREQ )
	{
		if ( val < 0 )
		{
			eprint( FATAL, "%s: Invalid negative RF frequency found in the "
					"table file `%s' at line %ld.\n", DEVICE_NAME,
					hp8647a.table_file, hp8647a_Lc );
			THROW( EXCEPTION );
		}

		if ( val > MAX_FREQ || val < MIN_FREQ )
			eprint( WARN, "%s: Frequency of %g MHz in table file `%s', line "
					"%ld,  is not within range of synthesizer (%f kHz - "
					"%f MHz).\n", DEVICE_NAME, val * 1.0e-6,
					hp8647a.table_file, hp8647a_Lc, MIN_FREQ * 1.0e-3,
					MAX_FREQ * 1.0e-6 );
	    hp8647a.att_table[ hp8647a.att_table_len ].freq = val;
		is_freq = SET;
	}
	else                // IS_ATT
	{
		if ( val > MIN_ATTEN - MAX_ATTEN )
		{
			eprint( FATAL, "%s: Attenuation of %f dB in table file `%s', "
					"line %ld, can't be achieved, maximum dynamic range is "
					"%f dB.\n", DEVICE_NAME, val, hp8647a.table_file,
					hp8647a_Lc, MIN_ATTEN - MAX_ATTEN );
			THROW( EXCEPTION );
		}
	    hp8647a.att_table[ hp8647a.att_table_len ].att = val;
		is_att = SET;
	}

	/* If both frequency and attenuation have been set increment entry count
	   and prepare for the next pair of data */

	if ( is_freq && is_att )
	{
		hp8647a.att_table_len++;
		is_freq = is_att = UNSET;
	}
}


/*-------------------------------------------------------------*/
/*-------------------------------------------------------------*/

static int hp8647a_comp( const void *a, const void *b )
{
	ATT_TABLE_ENTRY *A = ( ATT_TABLE_ENTRY * ) a,
					*B = ( ATT_TABLE_ENTRY * ) b;


	if ( A->freq == B->freq )
	{
		eprint( FATAL, "%s: Frequency %s Hz appears twice in table file "
				"`%s'\n", DEVICE_NAME, A->freq, hp8647a.table_file );
		THROW( EXCEPTION );
	}

	return A->freq < B->freq ? -1 : 1;
}
