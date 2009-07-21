/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with fsc2; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */


#include "fsc2_module.h"
#include <fftw3.h>

/* Include configuration file */

#include "fft.conf"

const char generic_type[ ] = DEVICE_TYPE;


Var_T * fft_name(           Var_T * /* v */ );
Var_T * fft_real(           Var_T * /* v */ );
Var_T * fft_power_spectrum( Var_T * /* v */ );
Var_T * fft_complex(        Var_T * /* v */ );



/*-----------------------------------*
 * Function returns the modules name
 *-----------------------------------*/

Var_T *
fft_name( Var_T * v  UNUSED_ARG )
{
    return vars_push( STR_VAR, DEVICE_NAME );
}


/*------------------------------------------------------------------*
 * Does a 1-dimensional DFT of real data. For a forward transform
 * expects just a 1-dimensioal array. For backwar transform accepts
 * either two 1-dimensional arrays or a single 2-dimensional one.
 * For a backward transform also another argument with the number
 * of points the resulting 1-dimensional array is to have must be
 * given (which, in turn, must be either twice the length of the
 * input arrays or one more).
 *------------------------------------------------------------------*/

Var_T *
fft_real( Var_T * v )
{
    Var_T *r = NULL,
          *c = NULL,
          *nv;
    int n;
    fftw_plan plan;
    double *dp;
    int i;
          

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

    /* If we get a single 1D array this is a forward transformation,
       if we get either a 2D array or two 1D arrays it's a backward
       transformation */

    if ( v->type & ( FLOAT_REF | INT_REF ) )
    {
		if (    v->dim != 2
			 || ! ( v->val.vptr[ 0 ]->type & ( FLOAT_ARR | INT_ARR ) )
			 || ! ( v->val.vptr[ 1 ]->type & ( FLOAT_ARR | INT_ARR ) ) )
		{
			print( FATAL, "Data argument(s) neither a 2D nor 2 1D arrays.\n" );
			THROW( EXCEPTION );
		}

        r = v->val.vptr[ 0 ];
        c = v->val.vptr[ 1 ];
        v = v->next;
    }
    else if ( v->type & ( FLOAT_ARR | INT_ARR ) )
    {
        if ( ! v->next )
        {
            r = v;
            v = v->next;
        }
        else if ( v->next->type & ( FLOAT_ARR | INT_ARR ) )
        {
            r = v;
            c = v->next;
            v= v->next->next;
        }
        else
        {
            print( FATAL, "Argument not either one or two 1D arrays or one "
                   "2D array.\n" );
            THROW( EXCEPTION );
        }
    }

    if ( ! c )     /* forward transformation */
    {
        double *in;
        fftw_complex *out = NULL;
        double *a,
               *b;
        double norm;


        CLOBBER_PROTECT( in );

        too_many_arguments( v );

        /* Sanity checks */

        if ( r->len <= 0 )
        {
            print( FATAL, "Input data array has no elements\n" );
            THROW( EXCEPTION );
        }

        if ( r->len > INT_MAX )
        {
            print( FATAL, "Input data array too long\n" );
            THROW( EXCEPTION );
        }

        n = r->len;

        /* Set up a pointer to the input data. If those are double values
           we can use the array from the variable directly, otherwise we
           need an extra array that we then have to initialize */

        if ( r->type == FLOAT_ARR )
            in = r->val.dpnt;
        else
        {
            long *from;

            if ( ! ( in = fftw_malloc( n * sizeof *in ) ) )
            {
                print( FATAL, "Running out of memory.\n" );
                THROW( OUT_OF_MEMORY_EXCEPTION );
            }

            dp = in;
            from = r->val.lpnt;
            
            for ( i = 0; i < n; i++ )
                *dp++ = *from++;
        }

        /* Allocate memory for the result data and make a plan */

        if (    ! ( out = fftw_malloc( ( n / 2 + 1 ) * sizeof *out ) )
             || ! ( plan = fftw_plan_dft_r2c_1d( n, in, out,
                                                 FFTW_ESTIMATE ) ) )
        {
            if ( out )
                fftw_free( out );
            if ( r->type == INT_ARR )
                fftw_free( in );
            print( FATAL, "Running out of memory.\n" );
            THROW( OUT_OF_MEMORY_EXCEPTION );
        }

        /* Do the FFT */

        fftw_execute( plan );

        /* Setup array to be returned */

        TRY
        {
            nv = vars_push_matrix( FLOAT_REF, 2, 2, n / 2 + 1 );
            TRY_SUCCESS;
        }
        OTHERWISE
        {
            fftw_destroy_plan( plan );
            fftw_free( out );
            if ( r->type == INT_ARR )
                fftw_free( in );
            RETHROW( );
        }

        /* Copy data over from result array into new variable */

        dp = ( double * ) out;
        a = nv->val.vptr[ 0 ]->val.dpnt;
        b = nv->val.vptr[ 1 ]->val.dpnt;
        norm = 1.0 / n;

        for ( i = 0; i < n / 2 + 1; i++ )
        {
            *a++ = norm * *dp++;
            *b++ = norm * *dp++;
        }

        /* Get rid of memory we used */

        fftw_destroy_plan( plan );
        fftw_free( out );
        if ( r->type == INT_ARR )
            fftw_free( in );
    }
    else           /* backward transformartion*/
    {
        fftw_complex *in = NULL;
        double *out = NULL;


        if ( ! v )
        {
            print( FATAL, "Missing output size argument.\n" );
            THROW( EXCEPTION );
        }

        if ( ! ( v->type & ( FLOAT_VAR | INT_VAR ) ) )
        {
            print( FATAL, "Invalid type of output size argument.\n" );
            THROW( EXCEPTION );
        }

        if (    ( v->type & FLOAT_VAR && v->val.dval > INT_MAX )
             && ( v->type & INT_VAR   && v->val.lval > INT_MAX ) )
        {
            print( FATAL, "Output size argument too large, must not larger "
                   "than %d.\n", INT_MAX );
            THROW( EXCEPTION );
        }

        n = v->type & FLOAT_VAR ? v->val.dval : v->val.lval;

        if ( r->len <= 0 )
        {
            print( FATAL, "Input data arrays have no elements\n" );
            THROW( EXCEPTION );
        }

        if ( r->len != c->len )
        {
            print( FATAL, "Input data arrays have different lengths.\n" );
            THROW( EXCEPTION );
        }

        if ( n != 2 * ( r->len - 1 ) && n != 2 * r->len - 1 )
        {
            print( FATAL, "Output size argument doesn't fit size of "
                   "input arrays, can only be %d or %d.\n",
                   2 * ( r->len - 1 ), 2 * r->len - 1, r->len );
            THROW( EXCEPTION );
        }

        if (    ! ( in = fftw_malloc( r->len * sizeof *in ) )
             || ! ( out = fftw_malloc( n * sizeof *out ) )
             || ! ( plan = fftw_plan_dft_c2r_1d( n, in, out,
                                                 FFTW_ESTIMATE ) ) )
        {
            if ( out )
                fftw_free( out );
            if ( in )
                fftw_free( in );
            print( FATAL, "Running out of memory.\n" );
            THROW( OUT_OF_MEMORY_EXCEPTION );
        }

        /* Set up the input array */

        dp = ( double * ) in;

        /* We need to distinguish four different types of combinations of
           input array types. That's a bit lengthy but not avoidable... */

        if ( r->type == FLOAT_ARR && c->type == FLOAT_ARR )
        {
            double *a = r->val.dpnt;
            double *b = c->val.dpnt;

            for ( i = 0; i < r->len; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else if ( r->type == FLOAT_ARR && c->type == INT_ARR )
        {
            double *a = r->val.dpnt;
            long *b = c->val.lpnt;

            for ( i = 0; i < r->len; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else if ( r->type == INT_ARR && c->type == FLOAT_ARR )
        {
            long *a = r->val.lpnt;
            double *b = c->val.dpnt;

            for ( i = 0; i < r->len; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else if ( r->type == INT_ARR && c->type == INT_ARR )
        {
            long *a = r->val.lpnt;
            long *b = c->val.lpnt;

            for ( i = 0; i < r->len; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }

        /* Do the FFT */

        fftw_execute( plan );

        /* Create a new variable with the output arrays data */

        nv = vars_push( FLOAT_ARR, out, n );

        /* Get rid of memory we used */

        fftw_destroy_plan( plan );
        fftw_free( out );
        fftw_free( in );
     }

    return nv;
}


/*-----------------------------------------------------------------*
 * Calculates the power spectrum of a real data array - basically
 * the same what fft_real() does in a forward transformation, just
 * with the squares of the magintudes of the frequency components
 * calculated afterwards and those returned to the caller.
 *-----------------------------------------------------------------*/

Var_T *
fft_power_spectrum( Var_T * v )
{
	fftw_complex *out;
	fftw_plan plan;
    double *in;
    double *dp,
           *to;
    Var_T *nv;
    int i;
    double norm;


	if ( v == NULL )
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

    if ( ! ( v->type & ( FLOAT_ARR | INT_ARR ) ) )
    {
        print( FATAL, "Argument isn't a 1D array.\n " );
        THROW( EXCEPTION );
    }

    if ( v->len <= 0 )
    {
        print( FATAL, "Input array is too short.\n" );
        THROW( EXCEPTION );
    }

    if ( v->len > INT_MAX )
    {
        print( FATAL, "Input array is too long.\n" );
        THROW( EXCEPTION );
    }

    if ( v->type == FLOAT_ARR )
        in = v->val.dpnt;
    else
    {
        long *a = v->val.lpnt;

        if ( ! ( in = fftw_malloc( v->len * sizeof *in ) ) )
        {
            print( FATAL, "Rnning outof memory.\n" );
            THROW( EXCEPTION );
        }

        for ( dp = in, i = 0; i < v->len; i++ )
            *dp++ = *a++;
    }

    if (    ! ( out = fftw_malloc( ( v->len / 2 + 1 ) * sizeof *out ) )
         || ! ( plan = fftw_plan_dft_r2c_1d( v->len, in, out,
                                             FFTW_ESTIMATE ) ) )
    {
        if ( out )
            fftw_free( out );
        if ( v->type == INT_ARR )
            fftw_free( in );
        print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
    }

    /* Do the FFT */

    fftw_execute( plan );

    if ( v->type == INT_ARR )
        fftw_free( in );

    /* Copy data over from result array into new variable */

    TRY
    {
        nv = vars_push( FLOAT_ARR, NULL, v->len / 2 + 1 );
        TRY_SUCCESS;
    }
    OTHERWISE
    {
        fftw_free( out );
        fftw_destroy_plan( plan );
        RETHROW( );
    }

    dp = ( double * ) out;
    to = nv->val.dpnt;
    norm = 1.0 / ( v->len * v->len );

    for ( i = 0; i < v->len / 2 + 1; dp++, i++ )
    {
        *to = *dp * *dp;
        dp++;
        *to -= *dp * *dp;
        *to++ *= norm;
    }

    fftw_free( out );
    fftw_destroy_plan( plan );

    return nv;
}


/*------------------------------------------------------------------*
 * Does a complex 1-dimensional DFT. Expects either a 2-dimensional
 * array or two 1-dimensional arrays as its input plus a string
 * with either "FORWARD" or "BACKWARD" (or a positive or negative
 * number) to specify the direction of the transformation. Returns
 * a 2-dimensional array.
 *------------------------------------------------------------------*/

Var_T *
fft_complex( Var_T * v )
{
	fftw_complex *data;
	fftw_plan     plan;
	Var_T        *r,
		         *c;
	double       *dp,
                 *rp,
		         *cp;
	int           dir;
	int           n;
	int           i;
	Var_T         *nv;


    CLOBBER_PROTECT( dp );
    CLOBBER_PROTECT( dir );

	if ( v == NULL )
	{
		print( FATAL, "Missing arguments\n" );
		THROW( EXCEPTION );
	}

	if ( v->type & ( FLOAT_REF | INT_REF ) )
	{
		if (    v->dim != 2
			 || ! ( v->val.vptr[ 0 ]->type & ( FLOAT_ARR | INT_ARR ) )
			 || ! ( v->val.vptr[ 1 ]->type & ( FLOAT_ARR | INT_ARR ) ) )
		{
			print( FATAL, "Data argument(s) neither a 2D nor 2 1D arrays.\n" );
			THROW( EXCEPTION );
		}

		r = v->val.vptr[ 0 ];
		c = v->val.vptr[ 1 ];
		v = v->next;
	}
	else if ( v->type & ( FLOAT_ARR | INT_ARR ) )
	{
		r = v;
		v = v->next;

		if ( ! v || ! ( v->type & ( FLOAT_ARR | INT_ARR ) ) )
		{
			print( FATAL, "Data argument(s) neither a 2D nor 2 1D arrays.\n" );
			THROW( EXCEPTION );
		}

		c = v;
		v = v->next;
	}
	else
	{
		print( FATAL, "Data argument(s) neither a 2D nor 2 1D arrays.\n" );
		THROW( EXCEPTION );
	}

	if ( r->len != c->len )
	{
		print( FATAL, "Lengths of first and second data array differ\n" );
		THROW( EXCEPTION );
	}

	if ( r->len <= 0 )
	{
		print( FATAL, "Input data arrays have no elements\n" );
		THROW( EXCEPTION );
	}

	n = r->len;

	if ( v == NULL )
	{
		print( SEVERE, "Missing FFT direction argument.\n" );
		THROW( EXCEPTION );
	}

	if ( v->type == STR_VAR )
	{
		if ( ! strcmp( v->val.sptr, "FORWARD" ) )
			dir = FFTW_FORWARD;
		else if ( ! strcmp( v->val.sptr, "BACKWARD" ) )
			dir = FFTW_BACKWARD;
		else
		{
			print( FATAL, "Invalid FFT direction argument, must be either "
				   "\"FORWARD\" (1) or \"BACKWARD\" (-1).\n" );
			THROW( EXCEPTION );
		}
	}
	else if ( v->type | ( INT_VAR | FLOAT_VAR ) )
	{
		if ( v->type == FLOAT_VAR )
			print( WARN, "Floating point number used as FFT direction "
				   "argument.\n" );

		if (    ( v->type == INT_VAR   && v->val.lval >= 0 )
			 || ( v->type == FLOAT_VAR && v->val.dval >= 0.0 ) )
			dir = FFTW_FORWARD;
		else if (    ( v->type == INT_VAR   && v->val.lval < 0 )
				  || ( v->type == FLOAT_VAR && v->val.dval < 0.0 ) )
			dir = FFTW_BACKWARD;
		else
		{
			print( FATAL, "Invalid FFT direction argument, must be either "
				   "\"FORWARD\" (1) or \"BACKWARD\" (-1).\n" );
			THROW( EXCEPTION );
		}
	}
	else
	{
		print( FATAL, "Invalid FFT direction argument, must be either "
			   "\"FORWARD\" (1) or \"BACKWARD\" (-1).\n" );
		THROW( EXCEPTION );
	}

	too_many_arguments( v );

	/* Create input/output array */

	if ( ! ( data  = fftw_malloc( n * sizeof *data  ) ) )
	{
		print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
	}

	/* Make a plan (input and output arrays must already be allocated) */

	if ( ! ( plan = fftw_plan_dft_1d( n, data, data, dir, FFTW_ESTIMATE ) ) )
	{
		fftw_free( data );
		print( FATAL, "Running out of memory.\n" );
        THROW( OUT_OF_MEMORY_EXCEPTION );
	}

	/* Copy data over to the input arrays. Two things have to be kept in
       mind: first of all the input arrays could be integers. And,
       second, for the backward transform the FFTW3 library expects the
       data starting with the null frequency, followed by increasing
       positive frequencies, then the most negative and all the remaining
       negative frequencies in ascending order. On the other hand the input
       arrays are supposed to start with the most negative frequencies and
       then ever increasing frequencies, ending with the largest positive
       frequency (with null in the middle). */

	if ( r->type == FLOAT_ARR && c->type == FLOAT_ARR )
	{
        double *a = r->val.dpnt,
			   *b = c->val.dpnt;

        if ( dir == FFTW_FORWARD )
        {
            dp = ( double * ) data;
            for ( i = 0; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else
        {
            dp = ( double * ) ( data + n / 2 + 1 );
            for ( i = 0; i < ( n - 1 ) / 2; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }

            dp = ( double * ) data;
            for ( ; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
	}
	else if ( r->type == FLOAT_ARR && c->type == INT_ARR )
	{
		double *a = r->val.dpnt;
        long   *b = c->val.lpnt;

        if ( dir == FFTW_FORWARD )
        {
            dp = ( double * ) data;
            for ( i = 0; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else
        {
            dp = ( double * ) ( data + n / 2 + 1);
            for ( i = 0; i < ( n - 1 ) / 2; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }

            dp = ( double * ) data;
            for ( ; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
    }
	else if ( r->type == INT_ARR && c->type == FLOAT_ARR )
	{
        long   *a = r->val.lpnt;
		double *b = c->val.dpnt;

        if ( dir == FFTW_FORWARD )
        {
            dp = ( double * ) data;
            for ( i = 0; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else
        {
            dp = ( double * ) ( data + n / 2 + 1);
            for ( i = 0; i < ( n - 1 ) / 2; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }

            dp = ( double * ) data;
            for ( ; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
    }
	else
	{
        long *a = r->val.lpnt,
             *b = c->val.lpnt;

        if ( dir == FFTW_FORWARD )
        {
            dp = ( double * ) data;
            for ( i = 0; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
        else
        {
            dp = ( double * ) ( data + n / 2 + 1);
            for ( i = 0; i < ( n - 1 ) / 2; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }

            dp = ( double * ) data;
            for ( ; i < n; i++ )
            {
                *dp++ = *a++;
                *dp++ = *b++;
            }
        }
    }

	/* Do the FFT */

	fftw_execute( plan );

	/* Setup array to be returned */

	TRY
	{
		nv = vars_push_matrix( FLOAT_REF, 2, 2, n );
		TRY_SUCCESS;
	}
	OTHERWISE
	{
		fftw_destroy_plan( plan );
		fftw_free( data );
		RETHROW( );
	}

    /* For a forward transformation we wantthe array start with the most
       negative frequency and the rest in strictly ascending order while
       the FFTW3 library returns the data in a different order, starting
       with the null frequency, then the positive frequencies and only
       the the negative ones, starting with the most negative one.
       It also returns unnormalized coefficients which we thus must scale. */

    rp = nv->val.vptr[ 0 ]->val.dpnt;
    cp = nv->val.vptr[ 1 ]->val.dpnt;

    if ( dir == FFTW_FORWARD )
    {
        double norm = 1.0 / n;

        dp = ( double * ) ( data + n / 2 + 1 );
        for ( i = 0; i < ( n - 1 ) / 2; i++ )
        {
            *rp++ = norm * *dp++;
            *cp++ = norm * *dp++;
        }

        dp = ( double * ) data;
        for ( ; i < n; i++ )
        {
            *rp++ = norm * *dp++;
            *cp++ = norm * *dp++;
        }
    }
    else
    {
        dp = ( double * ) data;

        for ( i = 0; i < n; i++ )
        {
            *rp++ = *dp++;
            *cp++ = *dp++;
        }
    }

	fftw_destroy_plan( plan );
	fftw_free( data );

	return nv;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
