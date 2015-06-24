#include "rs.h"


/*----------------------------------------------------*
 *----------------------------------------------------*/

void
inp_init( void )
{
	if ( FSC2_MODE != EXPERIMENT )
	{
		rs->inp.slope = SLOPE_POSITIVE;
		rs->inp.imp   = IMPEDANCE_HIGH;

		return;
	}

    rs->inp.slope = query_slope( "INP:TRIG:SLOPE?" );
    rs->inp.imp   = query_imp( "INP:MOD:IMP?" );

    if (    rs->inp.imp != IMPEDANCE_G600
         && rs->inp.imp != IMPEDANCE_HIGH )
		bad_data( );
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Slope
inp_slope( void )
{
	return rs->inp.slope;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Slope
inp_set_slope( enum Slope slope )
{
    if ( slope == rs->inp.slope )
        return rs->inp.slope;

	if ( slope != SLOPE_POSITIVE && slope != SLOPE_NEGATIVE )
	{
		print( FATAL, "Invalid trigger slope %d, use either \"POSITIVE\" or "
			   "\"NEGATIVE\".\n", slope );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->inp.slope - slope;

    char cmd[ 19 ] = "INP:TRIG:SLOPE ";
    strcat( cmd, slope == SLOPE_POSITIVE ? "POS" : "NEG" );

    rs_write( cmd );
    return rs->inp.slope = slope;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
inp_impedance( void )
{
	return rs->inp.imp;
}


/*----------------------------------------------------*
 *----------------------------------------------------*/

enum Impedance
inp_set_impedance( enum Impedance imp )
{
    if ( imp == rs->inp.imp )
        return rs->inp.imp;

    if ( imp != IMPEDANCE_G600 && imp != IMPEDANCE_HIGH )
	{
		print( FATAL, "Invalid input impedance %d requested, use either "
			   "\"G600\" or \"HIGH\".\n" );
		THROW( EXCEPTION );
	}

	if ( FSC2_MODE != EXPERIMENT )
		return rs->inp.imp = imp;

    char cmd[ 17 ] = "INP:MOD:IMP ";
    strcat( cmd, imp == IMPEDANCE_G600 ? "G600" : "HIGH" );

	rs_write( cmd );
    return rs->inp.imp = imp;
}


/*
 * Local variables:
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
