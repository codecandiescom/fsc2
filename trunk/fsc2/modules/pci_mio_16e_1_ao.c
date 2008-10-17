/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2008 Jens Thoms Toerring
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


#include "pci_mio_16e_1.h"


/*--------------------------------------------------------------------------*
 * Functions allows to reserve (or un-reserve) a DAC channel so that in the
 * following changes to the DAC channel require a pass-phrase (i.e. when
 * calling the function daq_ao_channel_setup and daq_set_voltage().
 *--------------------------------------------------------------------------*/

Var_T *
daq_reserve_dac( Var_T * v )
{
    bool lock_state = SET;
    long channel;
    int dac;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    channel = get_strict_long( v, "channel number" );

    dac = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                         "AO channel number" ),
                                        "AO channel" );

    v = vars_pop( v );

    if ( v == NULL )
        return vars_push( INT_VAR,
                         pci_mio_16e_1.ao_state.reserved_by[ dac ] ? 1L : 0L );

    if ( v->type != STR_VAR )
    {
        print( FATAL, "First argument isn't a string.\n" );
        THROW( EXCEPTION );
    }

    if ( v->next != NULL )
    {
        lock_state = get_boolean( v->next );
        too_many_arguments( v->next );
    }

    if ( pci_mio_16e_1.ao_state.reserved_by[ dac ] )
    {
        if ( lock_state == SET )
        {
            if ( ! strcmp( pci_mio_16e_1.ao_state.reserved_by[ dac ],
                           v->val.sptr ) )
                return vars_push( INT_VAR, 1L );
            else
                return vars_push( INT_VAR, 0L );
        }
        else
        {
            if ( ! strcmp( pci_mio_16e_1.ao_state.reserved_by[ dac ],
                           v->val.sptr ) )
            {
                pci_mio_16e_1.ao_state.reserved_by[ dac ] =
                            T_free( pci_mio_16e_1.ao_state.reserved_by[ dac ] );
                return vars_push( INT_VAR, 1L );
            }
            else
                return vars_push( INT_VAR, 0L );
        }
    }

    if ( ! lock_state )
        return vars_push( INT_VAR, 1L );

    pci_mio_16e_1.ao_state.reserved_by[ dac ] = T_strdup( v->val.sptr );
    return vars_push( INT_VAR, 1L );
}


/*--------------------------------------------------------------*
 * Function for setting up an AO channels, can set the polarity
 * and if an external reference is to be used
 *--------------------------------------------------------------*/

Var_T *
daq_ao_channel_setup( Var_T * v )
{
    int dac;
    int er;
    int pol;
    int other_count;
    char *pass = NULL;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    pol = -1;
    er = -1;

    if ( v != NULL && v->type == STR_VAR )
    {
        pass = T_strdup( v->val.sptr );
        if ( ( v = vars_pop( v ) ) == NULL )
        {
            print( FATAL, "Missing arguments.\n" );
            T_free( pass );
            THROW( EXCEPTION );
        }
    }

    dac = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                         "AO channel number" ),
                                        "AO channel" );

    if ( pci_mio_16e_1.ao_state.reserved_by[ dac ] )
    {
        if ( pass == NULL )
        {
            print( FATAL, "CH%ld is reserved, phase-phrase required.\n", dac );
            THROW( EXCEPTION );
        }
        else if ( strcmp( pci_mio_16e_1.ao_state.reserved_by[ dac ], pass ) )
        {
            print( FATAL, "CH%ld is reserved, wrong phase-phrase.\n", dac );
            T_free( pass );
            THROW( EXCEPTION );
        }
    }
    else if ( pass != NULL )
        print( WARN, "Pass-phrase for non-reserved CH%ld.\n", dac );

    if ( pass )
        T_free( pass );

    other_count = 0;
    while (    ( v = vars_pop( v ) ) != NULL
            && other_count++ < 2
            && v->type == STR_VAR )
    {
        if ( ! strcasecmp( v->val.sptr, "BIPOLAR" ) )
        {
            if ( pol != -1 )
            {
                print( FATAL, "Polarity for AO channel specified more "
                       "than once.\n" );
                THROW( EXCEPTION );
            }

            pol = NI_DAQ_BIPOLAR;
        }
        else if ( ! strcasecmp( v->val.sptr, "UNIPOLAR" ) )
        {
            if ( pol != -1 )
            {
                print( FATAL, "Polarity for AO channel specified more "
                       "than once.\n" );
                THROW( EXCEPTION );
            }

            if (    pci_mio_16e_1.ao_state.is_used[ dac ]
                 && pci_mio_16e_1.ao_state.volts[ dac ] <= 0.0 )
            {
                print( FATAL, "Can't switch to unipolar mode while output "
                       "voltage is negative.\n" );
                THROW( EXCEPTION );
            }

            pol = NI_DAQ_UNIPOLAR;
        }
        else if ( ! strcasecmp( v->val.sptr, "INTERNAL_REFERENCE" ) )
        {
            if ( er != -1 )
            {
                print( FATAL, "Reference type for AO channel "
                       "specified more than once.\n" );
                THROW( EXCEPTION );
            }

            er = NI_DAQ_DISABLED;
        }
        else if ( ! strcasecmp( v->val.sptr, "EXTERNAL_REFERENCE" ) )
        {
            if ( er != -1 )
            {
                print( FATAL, "Reference type for AO channel specified more "
                       "than once.\n" );
                THROW( EXCEPTION );
            }

            er = NI_DAQ_ENABLED;
        }
        else
        {
            print( FATAL, "Invalid argument.\n" );
            THROW( EXCEPTION );
        }
    }

    if ( pol == -1 && er == -1 )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    if ( pol == -1 )
        pol = pci_mio_16e_1.ao_state.polarity[ dac ];
    if ( er == -1 )
        er = pci_mio_16e_1.ao_state.external_reference[ dac ];

    too_many_arguments( v );

    if ( pci_mio_16e_1.ao_state.is_used[ dac ] )
    {
        double tmp_volts = 0.0;

        /* When switching between uni- and bipolar mode or external and
           internal reference set output voltage to zero */

        if (    (    pol != ( int ) pci_mio_16e_1.ao_state.polarity[ dac ]
                  || er !=
                     ( int ) pci_mio_16e_1.ao_state.external_reference[ dac ] )
             && FSC2_MODE == EXPERIMENT
             && ni_daq_ao( pci_mio_16e_1.board, 1, &dac, &tmp_volts ) < 0 )
        {
            print( FATAL, "AO channel setup failed.\n" );
            THROW( EXCEPTION );
        }
    }
        
    if ( FSC2_MODE == EXPERIMENT )
    {
        NI_DAQ_STATE l_er = er;
        NI_DAQ_BU_POLARITY l_pol = pol;

        raise_permissions( );
        ret = ni_daq_ao_channel_configuration( pci_mio_16e_1.board, 1,
                                               &dac, &l_er, &l_pol );
        lower_permissions( );

        switch ( ret )
        {
            case NI_DAQ_OK :
                break;

            case NI_DAQ_ERR_NER :
                print( FATAL, "Board does not allow use of external "
                       "reference for AO.\n" );
                THROW( EXCEPTION );

            case NI_DAQ_ERR_UAO :
                print( FATAL, "Board allows only bipolar AO.\n" );
                THROW( EXCEPTION );

            default :
                print( FATAL, "AO channel setup failed.\n" );
                THROW( EXCEPTION );
        }
    }

    if ( pci_mio_16e_1.ao_state.is_used[ dac ] )
    {
        if (    er == NI_DAQ_ENABLED
             && pci_mio_16e_1.ao_state.external_reference[ dac ]
                                                           == NI_DAQ_DISABLED )
        {
            pci_mio_16e_1.ao_state.volts[ dac ] *= 0.1;
            if (    FSC2_MODE == EXPERIMENT
                 && ni_daq_ao( pci_mio_16e_1.board, 1, &dac,
                               pci_mio_16e_1.ao_state.volts + dac ) < 0 )
            {
                print( FATAL, "AO channel setup failed.\n" );
                THROW( EXCEPTION );
            }
        }

        if (    er == NI_DAQ_DISABLED
             && pci_mio_16e_1.ao_state.external_reference[ dac ]
                                                            == NI_DAQ_ENABLED )
        {
            pci_mio_16e_1.ao_state.volts[ dac ] *= 10.0;
            if (    FSC2_MODE == EXPERIMENT
                 && ni_daq_ao( pci_mio_16e_1.board, 1, &dac,
                               pci_mio_16e_1.ao_state.volts + dac ) < 0 )
            {
                print( FATAL, "AO channel setup failed.\n" );
                THROW( EXCEPTION );
            }
        }

        if (    pol != ( int ) pci_mio_16e_1.ao_state.polarity[ dac ]
             && FSC2_MODE == EXPERIMENT
             && ni_daq_ao( pci_mio_16e_1.board, 1, &dac,
                           pci_mio_16e_1.ao_state.volts + dac ) < 0 )
        {
            print( FATAL, "AO channel setup failed.\n" );
            THROW( EXCEPTION );
        }
    }

    pci_mio_16e_1.ao_state.external_reference[ dac ] = ( NI_DAQ_STATE ) er;
    pci_mio_16e_1.ao_state.polarity[ dac ] = ( NI_DAQ_BU_POLARITY ) pol;

    return vars_push( INT_VAR, 1L );    
}


/*-----------------------------------------------------*
 * Function for outputting an voltage at an AO channel
 *-----------------------------------------------------*/

Var_T *
daq_set_voltage( Var_T * v )
{
    int dac;
    double volts;
    char *pass = NULL;
    int ret;


    if ( v == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    if ( v != NULL && v->type == STR_VAR )
    {
        pass = T_strdup( v->val.sptr );
        if ( ( v = vars_pop( v ) ) == NULL )
        {
            print( FATAL, "Missing arguments.\n" );
            THROW( EXCEPTION );
        }
    }

    dac = pci_mio_16e_1_channel_number( get_strict_long( v,
                                                         "AO channel number" ),
                                        "AO channel" );

    if ( pci_mio_16e_1.ao_state.reserved_by[ dac ] )
    {
        if ( pass == NULL )
        {
            print( FATAL, "CH%ld is reserved, phase-phrase required.\n", dac );
            THROW( EXCEPTION );
        }
        else if ( strcmp( pci_mio_16e_1.ao_state.reserved_by[ dac ], pass ) )
        {
            T_free( pass );
            print( FATAL, "CH%ld is reserved, wrong phase-phrase.\n", dac );
            THROW( EXCEPTION );
        }
    }

    T_free( pass );

    if ( ( v = vars_pop( v ) ) == NULL )
    {
        print( FATAL, "Missing arguments.\n" );
        THROW( EXCEPTION );
    }

    volts = get_double( v, "AO output voltage" );

    if (    pci_mio_16e_1.ao_state.polarity[ dac ] == NI_DAQ_UNIPOLAR
         && volts < 0.0 )
    {
        print( FATAL, "AO channel %d is configured for unipolar output, "
               "invalid negative voltage.\n", dac + 1 );
        THROW( EXCEPTION );
    }

    if (    pci_mio_16e_1.ao_state.external_reference[ dac ] == NI_DAQ_DISABLED
         && fabs( volts ) > 10.00001 )
    {
        print( FATAL, "Output voltage of %.f V not within allowed range.\n",
               volts );
        THROW( EXCEPTION );
    }
    else if (    pci_mio_16e_1.ao_state.external_reference[ dac ]
                                                              == NI_DAQ_ENABLED
              && fabs( volts ) > 1.00001 )
    {
        print( FATAL, "When using an external reference the output "
               "voltage argument must be within the range [ %d, 1 ].\n",
               pci_mio_16e_1.ao_state.polarity[ dac ] == NI_DAQ_UNIPOLAR ?
               0 : -1 );
        THROW( EXCEPTION );
    }

    if ( FSC2_MODE == EXPERIMENT )
    {
        raise_permissions( );
        ret = ni_daq_ao( pci_mio_16e_1.board, 1, &dac, &volts ); 
        lower_permissions( );

        if ( ret < 0 )
        {
            print( FATAL, "Setting AO voltage failed.\n" );
            THROW( EXCEPTION );
        }
    }

    pci_mio_16e_1.ao_state.volts[ dac ] = volts;
    pci_mio_16e_1.ao_state.is_used[ dac ] = SET;

    return vars_push( FLOAT_VAR, volts );
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
