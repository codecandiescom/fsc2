/*
  This program is free software; you can redistribute it and/or modify it
  under the same terms as Perl itself.

  Copyright (C) 2002-2008 Jens Thoms Toerring <jt@toerring.de>

  $Id$
*/


#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "FcntlLock.h"


MODULE = File::FcntlLock     PACKAGE = File::FcntlLock

PROTOTYPES: ENABLE


SV *
C_fcntl_lock( fd, function, flock_hash, int_err )
    int fd
    int function
    SV *flock_hash
    SV *int_err

    INIT:
        unsigned char flock_struct[ STRUCT_SIZE ];
        HV *fs;
        SV **sv_type, **sv_whence, **sv_start, **sv_len;

        sv_setiv( int_err, 0 );

        if ( ! SvROK( flock_hash ) )
        {
            sv_setiv( int_err, 1 );
            XSRETURN_UNDEF;
        }

        fs = ( HV * ) SvRV( flock_hash );

    CODE:
        /* Unfortunately, we can't even be sure that the constants aren't
           messed up... */

        switch ( function )
        {
            case F_GETLK :
                function = REAL_F_GETLK;
                break;

            case F_SETLK :
                function = REAL_F_SETLK;
                break;

            case F_SETLKW :
                function = REAL_F_SETLKW;
                break;
        }

        /* Let's be careful and not assume that anything at all will work as
           expected (otherwise we could merge this with the following) */

        if (    ( sv_type   = hv_fetch( fs, "l_type",   6, 0 ) ) == NULL
             || ( sv_whence = hv_fetch( fs, "l_whence", 8, 0 ) ) == NULL
             || ( sv_start  = hv_fetch( fs, "l_start",  7, 0 ) ) == NULL
             || ( sv_len    = hv_fetch( fs, "l_len",    5, 0 ) ) == NULL )
        {
            sv_setiv( int_err, 1 );
            XSRETURN_UNDEF;
        }

        /* Set up the the flock structure expected by fcntl(2) with the
           values in the hash we got passed */

        * ( LTYPE_TYPE * ) ( flock_struct + LTYPE_OFFSET ) =
                                           ( LTYPE_TYPE ) SvIV( *sv_type );
        * ( LWHENCE_TYPE * ) ( flock_struct + LWHENCE_OFFSET ) =
                                           ( LWHENCE_TYPE ) SvIV( *sv_whence );
        * ( LSTART_TYPE * ) ( flock_struct + LSTART_OFFSET ) =
                                           ( LSTART_TYPE ) SvIV( *sv_start );
        * ( LLEN_TYPE * ) ( flock_struct + LLEN_OFFSET ) =
                                           ( LLEN_TYPE ) SvIV( *sv_len );

        /* Now comes the great moment: fcntl(2) is finally called - if we want
           the lock immediately but some other process is holding it we return
           'undef' (people can find out about the reasons by checking errno).
           The same happens if we wait for the lock but receive a signal
           before we obtain the lock. */

        if ( fcntl( fd, function, flock_struct ) != 0 )
            XSRETURN_UNDEF;

        /* Now to find out who's holding the lock we now must unpack the
           structure we got back from fcntl(2) and store it in the hash we
           got passed. */

        if ( function == REAL_F_GETLK )
        {
            hv_store( fs, "l_type",   6, newSViv( * ( LTYPE_TYPE * ) 
                                      ( flock_struct + LTYPE_OFFSET ) ), 0 );
            hv_store( fs, "l_whence", 8, newSViv( * ( LWHENCE_TYPE * ) 
                                      ( flock_struct + LWHENCE_OFFSET ) ), 0 );
            hv_store( fs, "l_start",  7, newSViv( * ( LSTART_TYPE * ) 
                                      ( flock_struct + LSTART_OFFSET ) ), 0 );
            hv_store( fs, "l_len",    5, newSViv( * ( LLEN_TYPE * ) 
                                      ( flock_struct + LLEN_OFFSET ) ), 0 );
            hv_store( fs, "l_pid",    5, newSViv( * ( LPID_TYPE * ) 
                                      ( flock_struct + LPID_OFFSET ) ), 0 );
        }

        /* Return the systems return value of the fcntl(2) call (which is 0)
           but in a way that can't be mistaken as meaning false (shamelessly
           stolen from pp_sys.c in the the Perl sources). */

        RETVAL = newSVpvn( "0 but true", 10 );

    OUTPUT:
        RETVAL
