#include <stdio.h>
#include <stdlib.h>
#include <medriver/medriver.h>


struct dio_type {
	int type;
	const char *name;
} types[ ] = { { ME_TYPE_DI,  "DI" },
			   { ME_TYPE_DO,  "DO" },
			   { ME_TYPE_DIO, "DIO" } };


void query_subdevice( int dev_no, struct dio_type *type );


int
main( int    argc,
	  char * argv[ ] )
{
	int dev_no;
	char *ep;
	int retval;
	char desc[ ME_DEVICE_DESCRIPTION_MAX_COUNT ];
	size_t i;

	if ( argc < 2 || ! *argv[ 1 ] )
	{
		fprintf( stderr, "me_di_get_devices  DEVNO\n" );
		return EXIT_FAILURE;
	}

	dev_no = ( int ) strtol( argv[ 1 ], &ep, 10 );

	if ( dev_no < 0 || *ep )
	{
		fprintf( stderr, "me_di_get_devices  DEVNO\n" );
		return EXIT_FAILURE;
	}

    if ( ( retval = meOpen( ME_OPEN_NO_FLAGS ) ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        fprintf( stderr, "Failed to open device: %s\n", msg );
		return EXIT_FAILURE;
    }

	if ( ( retval = meQueryDescriptionDevice( dev_no, desc,
											ME_DEVICE_DESCRIPTION_MAX_COUNT ) )
		                                                  != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        fprintf( stderr, "Failed to open device: %s\n", msg );
		return EXIT_FAILURE;
    }

	printf( "%s\n\n", desc );

	for ( i = 0; i < sizeof types / sizeof *types; i++ )
		query_subdevice( dev_no, types + i );

    if ( ( retval = meClose( ME_CLOSE_NO_FLAGS ) ) != ME_ERRNO_SUCCESS )
    {
        char msg[ ME_ERROR_MSG_MAX_COUNT ];

        meErrorGetMessage( retval, msg, sizeof msg );
        fprintf( stderr, "Failed to close device: %s\n", msg );
    }

	return 0;
}


void
query_subdevice( int dev_no,
				 struct dio_type *type )
{
	int sd = 0;
	int retval;
	int ch;
	int caps;

	while ( 1 )
	{
		if ( ( retval = meQuerySubdeviceByType( dev_no, sd, type->type,
												ME_SUBTYPE_SINGLE, &sd ) )
		                                                  != ME_ERRNO_SUCCESS )
			return;

		if ( meQueryNumberChannels( dev_no, sd, &ch ) != ME_ERRNO_SUCCESS )
		{
			char msg[ ME_ERROR_MSG_MAX_COUNT ];

			meErrorGetMessage( retval, msg, sizeof msg );
			fprintf( stderr, "Failed to determine number of channels: %s\n",
					 msg );
			exit( EXIT_FAILURE );
		}

		if ( meQuerySubdeviceCaps( dev_no, sd, &caps ) != ME_ERRNO_SUCCESS )
		{
			char msg[ ME_ERROR_MSG_MAX_COUNT ];

			meErrorGetMessage( retval, msg, sizeof msg );
			fprintf( stderr, "Failed to determine capabilities: %s\n",
					 msg );
			exit( EXIT_FAILURE );
		}

		printf( "Subdevice %d:  TYPE = %3s  MAX_CHANNELS = %d  CAPABILITIES"
				" = %d\n", sd, type->name, ch, caps );
		sd++;
	}
}
