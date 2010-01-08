#include <stdio.h>
#include <stdlib.h>
#include <medriver/medriver.h>


struct me_types {
    int type;
    const char *name;
} types[ ] = { { ME_TYPE_DI,  "DI"  },
               { ME_TYPE_DO,  "DO"  },
               { ME_TYPE_DIO, "DIO" },
               { ME_TYPE_AI,  "AI"  },
               { ME_TYPE_AO,  "AO"  } };


static void
query_subdevice( int               dev_no,
                 struct me_types * type );


/*---------------------------------------------*
 *---------------------------------------------*/

int
main( int    argc,
      char * argv[ ] )
{
    int dev_no;
    char *ep;
    char desc[ ME_DEVICE_DESCRIPTION_MAX_COUNT ];
    size_t i;

    /* We need the device number as an argument */

    if ( argc < 2 || ! *argv[ 1 ] )
    {
        fprintf( stderr, "Usage: me_get_devices device-number\n" );
        return EXIT_FAILURE;
    }

    dev_no = ( int ) strtol( argv[ 1 ], &ep, 10 );

    if ( dev_no < 0 || *ep )
    {
        fprintf( stderr, "Invalid device number argument\n" );
        return EXIT_FAILURE;
    }

    /* Initialize Meilhaus library */

    if ( meOpen( ME_OPEN_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetLastMessage( msg, sizeof msg );
        fprintf( stderr, "Failed to open device: %s\n", msg );
        return EXIT_FAILURE;
    }

    /* Get description of device */

    if ( meQueryDescriptionDevice( dev_no, desc,
								   ME_DEVICE_DESCRIPTION_MAX_COUNT )
                                                           != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetLastMessage( msg, sizeof msg );
        fprintf( stderr, "Failed to open device: %s\n", msg );
        return EXIT_FAILURE;
    }

    printf( "%s\n\n", desc );

    /* Get information about the subdevices */

    for ( i = 0; i < sizeof types / sizeof *types; i++ )
        query_subdevice( dev_no, types + i );

    /* Shutdown Meilhaus library */

    if ( meClose( ME_CLOSE_NO_FLAGS ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetLastMessage( msg, sizeof msg );
        fprintf( stderr, "Failed to close device: %s\n", msg );
    }

    return 0;
}


/*---------------------------------------------*
 *---------------------------------------------*/

static void
query_subdevice( int               dev_no,
                 struct me_types * type )
{
    int sd = 0;
    int ch;
    int caps;

    /* Loop until no more subdevices of the requested type are found */

    while ( 1 )
    {
        if ( meQuerySubdeviceByType( dev_no, sd, type->type,
                                     ME_SUBTYPE_ANY, &sd ) != ME_ERRNO_SUCCESS )
            return;

        /* Get number of channels */

        if ( meQueryNumberChannels( dev_no, sd, &ch ) != ME_ERRNO_SUCCESS )
        {
            char msg[ ME_ERROR_MSG_MAX_COUNT ];

            meErrorGetLastMessage( msg, sizeof msg );
            fprintf( stderr, "Failed to determine number of channels: %s\n",
                     msg );
            exit( EXIT_FAILURE );
        }

        /* Query the capabilities */

        if ( meQuerySubdeviceCaps( dev_no, sd, &caps ) != ME_ERRNO_SUCCESS )
        {
            char msg[ ME_ERROR_MSG_MAX_COUNT ];

            meErrorGetLastMessage( msg, sizeof msg );
            fprintf( stderr, "Failed to determine capabilities: %s\n",
                     msg );
            exit( EXIT_FAILURE );
        }

        printf( "Subdevice %d:  TYPE = %3s  MAX_CHANNELS = %2d  CAPABILITIES"
                " = %2d\n", sd, type->name, ch, caps );

        /* If this is an AI or AO channel that has a FIFO report its size */

        if (    ( type->type == ME_TYPE_AI && caps & ME_CAPS_AI_FIFO )
             || ( type->type == ME_TYPE_AO && caps & ME_CAPS_AO_FIFO ) )
        {
            if ( meQuerySubdeviceCapsArgs( dev_no, sd,
										   type->type == ME_TYPE_AI ?
										   ME_CAP_AI_FIFO_SIZE :
										   ME_CAP_AO_FIFO_SIZE,
										   &caps, 1 ) != ME_ERRNO_SUCCESS )
            {
                char msg[ ME_ERROR_MSG_MAX_COUNT ];

                meErrorGetLastMessage( msg, sizeof msg );
                fprintf( stderr, "Failed to determine capability args for %s: "
                         "%s\n", type->type == ME_TYPE_AI ?
                         "ME_CAP_AI_FIFO_SIZE" : "ME_CAP_AO_FIFO_SIZE", msg );
            }
            else
                printf( "%s subdevice FIFO size: %d\n", type->name, caps );
        }

        sd++;
    }
}
