/*
  $Id$
*/


#include "fsc2.h"
#include <X11/Xresource.h>


static void set_up_app_options( FL_CMD_OPT app_opt[ ] );
static int fsc2_x_error_handler( Display *d, XErrorEvent *err );
static int fsc2_xio_error_handler( Display *d );


#if ( SIZE == HI_RES )
#define WIN_MIN_WIDTH    200
#define WIN_MIN_HEIGHT   370
#define NORMAL_FONT_SIZE FL_LARGE_SIZE
#define SMALL_FONT_SIZE  FL_SMALL_SIZE
#define SLIDER_SIZE      0.075
#else
#define WIN_MIN_WIDTH    200
#define WIN_MIN_HEIGHT   320
#define NORMAL_FONT_SIZE FL_SMALL_SIZE
#define SMALL_FONT_SIZE  FL_TINY_SIZE
#define SLIDER_SIZE      0.1
#endif


/* Some variables needed for the X resources */

#define N_APP_OPT 13
FL_IOPT xcntl;

char xGeoStr[ 64 ], xdisplayGeoStr[ 64 ],
	 xcutGeoStr[ 64 ], xtoolGeoStr[ 64 ],
	 xaxisFont[ 256 ];

int xbrowserfs, xbuttonfs, xinputfs, xlabelfs, xchoicefs, xsliderfs,
	xfileselectorfs, xhelpfs;

FL_resource xresources[ N_APP_OPT ] = {
	{ "geometry", "*.geometry", FL_STRING, xGeoStr, "", 64 },
    { "browserFontSize", "*.browserFontSize", FL_INT, &xbrowserfs,
	  "0", sizeof( int ) },
    { "buttonFontSize", "*.buttonFontSize", FL_INT, &xbuttonfs,
	  "0", sizeof( int ) },
    { "inputFontSize", "*.inputFontSize", FL_INT, &xinputfs,
	  "0", sizeof( int ) },
    { "labelFontSize", "*.labelFontSize", FL_INT, &xlabelfs,
	  "0", sizeof( int ) },
    { "displayGeometry", "*.displayGeometry", FL_STRING, xdisplayGeoStr,
	  "", 64 },
    { "cutGeometry", "*.cutGeometry", FL_STRING, xcutGeoStr, "", 64 },
    { "toolGeometry", "*.toolGeometry", FL_STRING, xtoolGeoStr, "", 64 },
	{ "axisFont", "*.axisFont", FL_STRING, xaxisFont, "", 256 },
    { "choiceFontSize", "*.choiceFontSize", FL_INT, &xchoicefs,
	  "0", sizeof( int ) },
    { "sliderFontSize", "*.sliderFontSize", FL_INT, &xsliderfs,
	  "0", sizeof( int ) },
    { "filselectorFontSize", "*.fileselectorFontSize", FL_INT,
	  &xfileselectorfs, "0", sizeof( int ) },
    { "helpFontSize", "*.helpFontSize", FL_INT, &xhelpfs, "0", sizeof( int ) },
};



/*-------------------------------------------------*/
/* xforms_init() registers the program with XFORMS */
/* and creates all the forms needed by the program.*/
/*-------------------------------------------------*/

bool xforms_init( int *argc, char *argv[] )
{
	Display *display;
	FL_Coord h, H;
	FL_Coord x1, y1, w1, h1, x2, y2, w2, h2;
	int i;
	int flags, wx, wy, ww, wh;
	XFontStruct *font;
	FL_CMD_OPT app_opt[ N_APP_OPT ];


	set_up_app_options( app_opt );

	if ( ( display = fl_initialize( argc, argv, "Fsc2", app_opt, N_APP_OPT ) )
		 == NULL )
		return FAIL;

	fl_get_app_resources( xresources, N_APP_OPT );

	for ( i = 0; i < N_APP_OPT; i++ )
	{
		T_free( app_opt[ i ].option );
		T_free( app_opt[ i ].specifier );
	}

//	XSetErrorHandler( fsc2_x_error_handler );
//	XSetIOErrorHandler( fsc2_xio_error_handler );

	/* Set some properties of goodies */

	fl_set_goodies_font( FL_NORMAL_STYLE, NORMAL_FONT_SIZE );
	fl_set_oneliner_font( FL_NORMAL_STYLE, NORMAL_FONT_SIZE );

	if ( * ( ( int * ) xresources[ HELPFONTSIZE ].var ) != 0 )
		fl_set_tooltip_font( FL_NORMAL_STYLE, 
							 * ( ( int * ) xresources[ HELPFONTSIZE ].var ) );
	else
		fl_set_tooltip_font( FL_NORMAL_STYLE, SMALL_FONT_SIZE );

	if ( * ( ( int * ) xresources[ FILESELFONTSIZE ].var ) != 0 )
		fl_set_fselector_fontsize( * ( ( int * )
									   xresources[ FILESELFONTSIZE ].var )  );
	else
		fl_set_fselector_fontsize( NORMAL_FONT_SIZE );
	fl_set_tooltip_color( FL_BLACK, FL_YELLOW );

	fl_disable_fselector_cache( 1 );
//	fl_set_fselector_placement( FL_PLACE_MOUSE | FL_FREE_SIZE );
	fl_set_fselector_border( FL_TRANSIENT );

	/* Set default font sizes */

	xcntl.browserFontSize = NORMAL_FONT_SIZE;
	xcntl.buttonFontSize  = NORMAL_FONT_SIZE;
	xcntl.inputFontSize   = NORMAL_FONT_SIZE;
	xcntl.labelFontSize   = NORMAL_FONT_SIZE;
	xcntl.choiceFontSize  = NORMAL_FONT_SIZE;
	xcntl.sliderFontSize  = NORMAL_FONT_SIZE;

	/* Set the default font size for browsers */

	if ( * ( ( int * ) xresources[ BROWSERFONTSIZE ].var ) != 0 )
		xcntl.browserFontSize =
			* ( ( int * ) xresources[ BROWSERFONTSIZE ].var );
	fl_set_defaults( FL_PDBrowserFontSize, &xcntl );

	/* Set the default font size for buttons */

	if ( * ( ( int * ) xresources[ BUTTONFONTSIZE ].var ) != 0 )
		xcntl.buttonFontSize =
			* ( ( int * ) xresources[ BUTTONFONTSIZE ].var );
	fl_set_defaults( FL_PDButtonFontSize, &xcntl );

	/* Set the default font size for input objects */

	if ( * ( ( int * ) xresources[ INPUTFONTSIZE ].var ) != 0 )
		xcntl.inputFontSize = * ( ( int * ) xresources[ INPUTFONTSIZE ].var );
	fl_set_defaults( FL_PDInputFontSize, &xcntl );

	/* Set the default font size for label and texts */

	if ( * ( ( int * ) xresources[ LABELFONTSIZE ].var ) != 0 )
		xcntl.labelFontSize = * ( ( int * ) xresources[ LABELFONTSIZE ].var );
	fl_set_defaults( FL_PDLabelFontSize, &xcntl );

	/* Set the default font size for choice objects */

	if ( * ( ( int * ) xresources[ CHOICEFONTSIZE ].var ) != 0 )
		xcntl.choiceFontSize =
			* ( ( int * ) xresources[ CHOICEFONTSIZE ].var );
	fl_set_defaults( FL_PDChoiceFontSize, &xcntl );

	/* Set the default font size for slider */

	if ( * ( ( int * ) xresources[ SLIDERFONTSIZE ].var ) != 0 )
		xcntl.sliderFontSize =
			* ( ( int * ) xresources[ SLIDERFONTSIZE ].var );
	fl_set_defaults( FL_PDSliderFontSize, &xcntl );

	/* Create and display the main form */

	main_form = create_form_fsc2( );

	fl_set_object_helper( main_form->Load, "Load new EDL program" );
	fl_set_object_helper( main_form->Edit, "Edit loaded EDL program" );
	fl_set_object_helper( main_form->reload, "Reload EDL program" );
	fl_set_object_helper( main_form->test_file, "Start syntax and "
						  "plausibility check" );
	fl_set_object_helper( main_form->run, "Start loaded EDL program" );
	fl_set_object_helper( main_form->quit, "Quit fsc2" );
	fl_set_object_helper( main_form->help, "Show documentation" );
	fl_set_object_helper( main_form->bug_report, "Mail a bug report" );

	fl_set_browser_fontstyle( main_form->browser, FL_FIXED_STYLE );
	fl_set_browser_fontstyle( main_form->error_browser, FL_FIXED_STYLE );

	fl_get_object_geometry( main_form->browser, &x1, &y1, &w1, &h1 );
	fl_get_object_geometry( main_form->error_browser, &x2, &y2, &w2, &h2 );

	h = y2 - y1 - h1;
	H = h1 + h2 + h;

	fl_set_slider_size( main_form->win_slider, SLIDER_SIZE );
	fl_set_slider_value( main_form->win_slider,
						 ( double ) ( h1 + h / 2 - 0.5 * H * SLIDER_SIZE )
						 / ( ( 1.0 - SLIDER_SIZE ) * H ) );

	/* Now show the form, taking user wishes about the geometry into account */

	if ( * ( ( char * ) xresources[ GEOMETRY ].var ) != '\0' )
	{
		flags = XParseGeometry( ( char * ) xresources[ GEOMETRY ].var,
								&wx, &wy, &ww, &wh );
		if ( WidthValue & flags && HeightValue & flags )
		{
			if ( ww < WIN_MIN_WIDTH )
				ww = WIN_MIN_WIDTH;
			if ( wh < WIN_MIN_HEIGHT )
				wh = WIN_MIN_HEIGHT;

			fl_set_form_size( main_form->fsc2, ww, wh );
		}

		if ( XValue & flags && YValue & flags )
		{
			fl_set_form_position( main_form->fsc2, wx, wy );
			fl_show_form( main_form->fsc2, FL_PLACE_GEOMETRY,
						  FL_FULLBORDER, "fsc2" );
		}
		else
			fl_show_form( main_form->fsc2, FL_PLACE_MOUSE | FL_FREE_SIZE,
						  FL_FULLBORDER, "fsc2" );
	}
	else
		fl_show_form( main_form->fsc2, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "fsc2" );

	fl_winminsize( main_form->fsc2->window, WIN_MIN_WIDTH, WIN_MIN_HEIGHT );

	/* Check if axis font exists (if the user set a font) */

	if ( * ( ( char * ) xresources[ AXISFONT ].var ) != '\0' )
	{
		if ( ( font = XLoadQueryFont( display,
									  ( char * ) xresources[ AXISFONT ].var ) )
			 == NULL )
			fprintf( stderr, "Error: Font '%s' not found.\n",
					 ( char * ) xresources[ AXISFONT ].var );
		else
			XFreeFont( display, font );
	}

	/* Set close handler for main form */

	fl_set_form_atclose( main_form->fsc2, main_form_close_handler, NULL );

	/* Set c_cdata and u_cdata elements of load button structure */

	main_form->Load->u_ldata = 0;
	main_form->Load->u_cdata = NULL;

	/* Create the form for writing a comment */

	input_form = create_form_input_form( );

	return OK;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void set_up_app_options( FL_CMD_OPT app_opt[ ] )
{
	app_opt[ GEOMETRY ].option            = T_strdup( "-geometry" );
	app_opt[ GEOMETRY ].specifier         = T_strdup( "*.geometry" );
	app_opt[ GEOMETRY ].argKind           = XrmoptionSepArg;
	app_opt[ GEOMETRY ].value             = ( caddr_t ) NULL;

	app_opt[ BROWSERFONTSIZE ].option     =  T_strdup( "-browserFontSize" );
	app_opt[ BROWSERFONTSIZE ].specifier  = T_strdup( "*.browserFontSize" );
	app_opt[ BROWSERFONTSIZE ].argKind    = XrmoptionSepArg;
	app_opt[ BROWSERFONTSIZE ].value      = ( caddr_t ) "0";

	app_opt[ BUTTONFONTSIZE	].option      = T_strdup( "-buttonFontSize" );
	app_opt[ BUTTONFONTSIZE	].specifier   = T_strdup( "*.buttonFontSize" );
	app_opt[ BUTTONFONTSIZE	].argKind     = XrmoptionSepArg;
	app_opt[ BUTTONFONTSIZE	].value       = ( caddr_t ) "0";

	app_opt[ INPUTFONTSIZE ].option       = T_strdup( "-inputFontSize" );
	app_opt[ INPUTFONTSIZE ].specifier    = T_strdup( "*.inputFontSize" );
	app_opt[ INPUTFONTSIZE ].argKind      = XrmoptionSepArg;
	app_opt[ INPUTFONTSIZE ].value        = ( caddr_t ) "0";

	app_opt[ LABELFONTSIZE ].option       = T_strdup( "-labelFontSize" );
	app_opt[ LABELFONTSIZE ].specifier    = T_strdup( "*.labelFontSize" );
	app_opt[ LABELFONTSIZE ].argKind      = XrmoptionSepArg;
	app_opt[ LABELFONTSIZE ].value        = ( caddr_t ) "0";

	app_opt[ DISPLAYGEOMETRY ].option     = T_strdup( "-displayGeometry" );
	app_opt[ DISPLAYGEOMETRY ].specifier  = T_strdup( "*.displayGeometry" );
	app_opt[ DISPLAYGEOMETRY ].argKind    = XrmoptionSepArg;
	app_opt[ DISPLAYGEOMETRY ].value      = ( caddr_t ) NULL;

	app_opt[ CUTGEOMETRY ].option         = T_strdup( "-cutGeometry" );
	app_opt[ CUTGEOMETRY ].specifier      = T_strdup( "*.cutGeometry" );
	app_opt[ CUTGEOMETRY ].argKind        = XrmoptionSepArg;
	app_opt[ CUTGEOMETRY ].value          = ( caddr_t ) NULL;

	app_opt[ TOOLGEOMETRY ].option        = T_strdup( "-toolGeometry" );
	app_opt[ TOOLGEOMETRY ].specifier     = T_strdup( "*.toolGeometry" );
	app_opt[ TOOLGEOMETRY ].argKind       = XrmoptionSepArg;
	app_opt[ TOOLGEOMETRY ].value         = ( caddr_t ) NULL;

	app_opt[ AXISFONT ].option            = T_strdup( "-axisFont" );
	app_opt[ AXISFONT ].specifier         = T_strdup( "*.axisFont" );
	app_opt[ AXISFONT ].argKind           = XrmoptionSepArg;
	app_opt[ AXISFONT ].value             = ( caddr_t ) NULL;

	app_opt[ CHOICEFONTSIZE ].option      = T_strdup( "-choiceFontSize" );
	app_opt[ CHOICEFONTSIZE ].specifier   = T_strdup( "*.choiceFontSize" );
	app_opt[ CHOICEFONTSIZE ].argKind     = XrmoptionSepArg;
	app_opt[ CHOICEFONTSIZE ].value       = ( caddr_t ) "0";

	app_opt[ SLIDERFONTSIZE ].option      = T_strdup( "-sliderFontSize" );
	app_opt[ SLIDERFONTSIZE ].specifier   = T_strdup( "*.sliderFontSize" );
	app_opt[ SLIDERFONTSIZE ].argKind     = XrmoptionSepArg;
	app_opt[ SLIDERFONTSIZE ].value       = ( caddr_t ) "0";

	app_opt[ FILESELFONTSIZE ].option     =
		                                   T_strdup( "-fileselectorFontSize" );
	app_opt[ FILESELFONTSIZE ].specifier  =
		                                  T_strdup( "*.fileselectorFontSize" );
	app_opt[ FILESELFONTSIZE ].argKind    = XrmoptionSepArg;
	app_opt[ FILESELFONTSIZE ].value      = ( caddr_t ) "0";

	app_opt[ HELPFONTSIZE ].option        = T_strdup( "-helpFontSize" );
	app_opt[ HELPFONTSIZE ].specifier     = T_strdup( "*.helpFontSize" );
	app_opt[ HELPFONTSIZE ].argKind       = XrmoptionSepArg;
	app_opt[ HELPFONTSIZE ].value         = ( caddr_t ) "0";
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int main_form_close_handler( FL_FORM *a, void *b )
{
	a = a;
	b = b;

	if ( main_form->quit->active != 1 )
		return FL_IGNORE;

	clean_up( );
	xforms_close( );

	exit( EXIT_SUCCESS );
	return FL_OK;
}


/*-----------------------------------------*/
/* xforms_close() closes and deletes all   */
/* forms previously needed by the program. */
/*-----------------------------------------*/

void xforms_close( void )
{
	if ( fl_form_is_visible( main_form->fsc2 ) )
		fl_hide_form( main_form->fsc2 );
	fl_free_form( main_form->fsc2 );
}


/*------------------------------------------------------------*/  
/* Callback function for movements of the slider that adjusts */
/* the sizes of the program and the error/output browser      */
/*------------------------------------------------------------*/  

void win_slider_callback( FL_OBJECT *a, long b )
{
	FL_Coord h, H;
	FL_Coord new_h1 ;
	FL_Coord x1, y1, w1, h1, x2, y2, w2, h2;


	b = b;

	fl_freeze_form( main_form->fsc2 );

	fl_get_object_geometry( main_form->browser, &x1, &y1, &w1, &h1 );
	fl_get_object_geometry( main_form->error_browser, &x2, &y2, &w2, &h2 );

	h = y2 - y1 - h1;
	H = y2 - y1 + h2;

	new_h1 = ( FL_Coord ) ( ( 1.0 - SLIDER_SIZE ) * H 
							* fl_get_slider_value( a )
							+ 0.5 * H * SLIDER_SIZE - h / 2 );

	fl_set_object_size( main_form->browser, w1, new_h1 );
	fl_set_object_geometry( main_form->error_browser, x2, y1 + new_h1 + h,
							w2, H - ( new_h1 + h ) );
	fl_unfreeze_form( main_form->fsc2 );
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

static int fsc2_x_error_handler( Display *d, XErrorEvent *err )
{
	char err_str[ 1024 ];


	XGetErrorText( d, err->error_code, err_str, 1024 );
	fprintf( stderr, "fsc2 (%d) killed by an X error: %s.\n",
			 getpid( ), err_str );
	exit( EXIT_FAILURE );
}


/*------------------------------------------------------------*/  
/*------------------------------------------------------------*/  

static int fsc2_xio_error_handler( Display *d )
{
	d = d;
	fprintf( stderr, "fsc2 (%d) killed by a fatal X error.\n", getpid( ) );
	exit( EXIT_FAILURE );
}
