/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


#include "tegam2714a_p.h"


static void tegam2714a_p_init_print( FILE * fp );

static void tegam2714a_p_basic_check( void );


/*-------------------------------------------------------------*
 * Function does everything that needs to be done for checking
 * and completing the internal representation of the pulser at
 * the start of a test run.
 *-------------------------------------------------------------*/

void tegam2714a_p_init_setup( void )
{
    tegam2714a_p_basic_check( );

    if ( tegam2714a_p.dump_file != NULL )
    {
        tegam2714a_p_init_print( tegam2714a_p.dump_file );
        tegam2714a_p_write_pulses( tegam2714a_p.dump_file );
    }
    if ( tegam2714a_p.show_file != NULL )
    {
        tegam2714a_p_init_print( tegam2714a_p.show_file );
        tegam2714a_p_write_pulses( tegam2714a_p.show_file );
    }
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void tegam2714a_p_init_print( FILE * fp )
{
    Function_T *f = &tegam2714a_p.function;


    if ( fp == NULL )
        return;

    fprintf( fp, "TB: %g\nD: 0\n===\n", tegam2714a_p.timebase );

	if ( ! f->is_used )
		return;

	fprintf( fp, "%s:%d %ld%s\n", f->name, tegam2714a_p.function.channel,
              tegam2714a_p_double2ticks( f->delay ),
             f->is_inverted ? " I" : "" );
}


/*--------------------------------------------------------------------------*
 *--------------------------------------------------------------------------*/

static void tegam2714a_p_basic_check( void )
{
    Pulse_T *p;
    Function_T *f = &tegam2714a_p.function;



    if ( tegam2714a_p.pulses == NULL )
    {
        print( SEVERE, "No pulses have been defined.\n" );
        return;
    }

    for ( p = tegam2714a_p.pulses; p != NULL; p = p->next )
    {
        p->is_active = IS_ACTIVE( p );

        /* Check the pulse function */

        if ( ! p->is_function )
        {
            print( FATAL, "Pulse #%ld is not associated with a function.\n",
                   p->num );
            THROW( EXCEPTION );
        }

        /* Check start position and pulse length */

        if ( p->is_active &&
             p->pos + p->len + f->delay >= MAX_PULSER_BITS )
        {
            print( FATAL, "Pulse #%ld does not fit into the pulsers memory.\n",
                   p->num );
            THROW( EXCEPTION );
        }

        f->pulses = PULSE_PP T_realloc( f->pulses,
                                        ++f->num_pulses * sizeof * f->pulses );
        f->pulses[ f->num_pulses - 1 ] = p;
    }

    if ( ! ( f->is_used = f->num_pulses != 0 ) )
		print( WARN, "No pulses have been assigned to function '%s'.\n",
			   f->name );
    else
        tegam2714a_p_do_checks( SET );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
