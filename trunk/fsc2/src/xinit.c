/*
  $Id$

  Copyright (C) 1999-2002 Jens Thoms Toerring

  This file is part of fsc2.

  Fsc2 is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.

  Fsc2 is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with fsc2; see the file COPYING.  If not, write to
  the Free Software Foundation, 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
*/


#include "fsc2.h"
#include <X11/Xresource.h>
#include <dlfcn.h>


static int main_form_close_handler( FL_FORM *a, void *b );
static void setup_app_options( FL_CMD_OPT app_opt[ ] );
static bool dl_fsc2_rsc( void );

#if 0
static int fsc2_x_error_handler( Display *d, XErrorEvent *err );
static int fsc2_xio_error_handler( Display *d );
#endif


/* Some variables needed for the X resources */

#define N_APP_OPT 17
FL_IOPT xcntl;

char xGeoStr[ 64 ], xdisplayGeoStr[ 64 ],
	 xcutGeoStr[ 64 ], xtoolGeoStr[ 64 ],
	 xaxisFont[ 256 ], xsmb[ 64 ], xsizeStr[ 64 ];

int xbrowserfs, xbuttonfs, xinputfs, xlabelfs, xchoicefs, xsliderfs,
	xfileselectorfs, xhelpfs, xnocm, xnob, xport;

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
	{ "stopMouseButton", "*.stopMouseButton", FL_STRING, &xsmb, "", 64 },
	{ "noCrashMail", "*.noCrashMail", FL_BOOL, &xnocm, "0", sizeof( int ) },
	{ "size", "*.size", FL_STRING, xsizeStr, "", 64 },
    { "httpPort", "*.httpPort", FL_INT, &xport, "0", sizeof( int ) }
};


static struct {
	unsigned int WIN_MIN_WIDTH;
	unsigned int WIN_MIN_HEIGHT;
	int NORMAL_FONT_SIZE;
	int SMALL_FONT_SIZE;
	int TOOLS_FONT_SIZE;
	double SLIDER_SIZE;
} XI_sizes;



/*-------------------------------------------------*/
/* xforms_init() registers the program with XFORMS */
/* and creates all the forms needed by the program.*/
/*-------------------------------------------------*/

bool xforms_init( int *argc, char *argv[ ] )
{
	Display *display;
	FL_Coord h, H;
	FL_Coord cx1, cy1, cw1, ch1, cx2, cy2, cw2, ch2;
	int i;
	int flags, wx, wy;
	unsigned int ww, wh;
	XFontStruct *font;
	FL_CMD_OPT app_opt[ N_APP_OPT ];
	volatile bool needs_pos = UNSET;
	XWindowAttributes attr;
	Window root, parent, *children;
	unsigned int nchilds;
#if defined WITH_HTTP_SERVER
	char *www_help;
#endif

	setup_app_options( app_opt );

	if ( ( display = fl_initialize( argc, argv, "Fsc2", app_opt, N_APP_OPT ) )
		 == NULL )
		return FAIL;

	/* xforms sets the locale to the one set in the environment variables. But
	   we need at least the "normal" formatting of numeric values, otherwise
	   functions like strtod() work not as expected. */

	setlocale( LC_NUMERIC, "C" );

	/* We also must keep the main window from always becoming the topmost
	   window at the most inconvenient moments... */

	fl_set_app_nomainform( 1 );

	/* All command line flags shold have been dealt with now. */

	if ( *argc > 1 && argv[ 1 ][ 0 ] == '-' )
	{
		fprintf( stderr, "Unknown option \"%s\".\n", argv[ 1 ] );
		usage( EXIT_FAILURE );
	}

	fl_get_app_resources( xresources, N_APP_OPT );

	for ( i = 0; i < N_APP_OPT; i++ )
	{
		T_free( app_opt[ i ].option );
		T_free( app_opt[ i ].specifier );
	}

#if 0
	/* Maybe we'll use homegrown X error handlers sometime, but currently
	   there's no reason to do so... */

	XSetErrorHandler( fsc2_x_error_handler );
	XSetIOErrorHandler( fsc2_xio_error_handler );
#endif

	/* Find out the resolution we're going to run in */

	if ( fl_scrh >= 870 && fl_scrw >= 1152 )
		GUI.G_Funcs.size = ( bool ) HIGH;
	else
		GUI.G_Funcs.size = ( bool ) LOW;

	if ( * ( ( char * ) xresources[ RESOLUTION ].var ) != '\0' )
	{
		if ( ! strcasecmp( ( char * ) xresources[ RESOLUTION ].var, "s" ) ||
			 ! strcasecmp( ( char * ) xresources[ RESOLUTION ].var, "small" ) )
			GUI.G_Funcs.size = ( bool ) LOW;
		if ( ! strcasecmp( ( char * ) xresources[ RESOLUTION ].var, "l" ) ||
			 ! strcasecmp( ( char * ) xresources[ RESOLUTION ].var, "large" ) )
			GUI.G_Funcs.size = ( bool ) HIGH;
	}

	if ( GUI.G_Funcs.size == LOW )
	{
		XI_sizes.WIN_MIN_WIDTH    = 200;
		XI_sizes.WIN_MIN_HEIGHT   = 320;
		XI_sizes.NORMAL_FONT_SIZE = FL_SMALL_SIZE;
		XI_sizes.SMALL_FONT_SIZE  = FL_TINY_SIZE;
		XI_sizes.TOOLS_FONT_SIZE  = FL_SMALL_FONT;
		XI_sizes.SLIDER_SIZE      = 0.1;
	}
	else
	{
		XI_sizes.WIN_MIN_WIDTH    = 220;
		XI_sizes.WIN_MIN_HEIGHT   = 570;
		XI_sizes.NORMAL_FONT_SIZE = FL_MEDIUM_SIZE;
		XI_sizes.SMALL_FONT_SIZE  = FL_SMALL_SIZE;
		XI_sizes.TOOLS_FONT_SIZE  = FL_MEDIUM_FONT;
		XI_sizes.SLIDER_SIZE      = 0.075;
	}

	/* Set some properties of goodies */

	fl_set_goodies_font( FL_NORMAL_STYLE, XI_sizes.NORMAL_FONT_SIZE );
	fl_set_oneliner_font( FL_NORMAL_STYLE, XI_sizes.NORMAL_FONT_SIZE );

	if ( * ( ( int * ) xresources[ HELPFONTSIZE ].var ) != 0 )
		fl_set_tooltip_font( FL_NORMAL_STYLE,
							 * ( ( int * ) xresources[ HELPFONTSIZE ].var ) );
	else
		fl_set_tooltip_font( FL_NORMAL_STYLE, XI_sizes.SMALL_FONT_SIZE );

	if ( * ( ( int * ) xresources[ FILESELFONTSIZE ].var ) != 0 )
		fl_set_fselector_fontsize( * ( ( int * )
									   xresources[ FILESELFONTSIZE ].var )  );
	else
		fl_set_fselector_fontsize( XI_sizes.NORMAL_FONT_SIZE );
	fl_set_tooltip_color( FL_BLACK, FL_YELLOW );

	fl_disable_fselector_cache( 1 );
	fl_set_fselector_border( FL_TRANSIENT );

	/* Set default font sizes */

	xcntl.browserFontSize = XI_sizes.NORMAL_FONT_SIZE;
	xcntl.buttonFontSize  = XI_sizes.TOOLS_FONT_SIZE;
	xcntl.inputFontSize   = XI_sizes.TOOLS_FONT_SIZE;
	xcntl.labelFontSize   = XI_sizes.TOOLS_FONT_SIZE;
	xcntl.choiceFontSize  = XI_sizes.TOOLS_FONT_SIZE;
	xcntl.sliderFontSize  = XI_sizes.TOOLS_FONT_SIZE;

	/* Set the stop mouse button (only used in the 'Display' window) */

	GUI.stop_button_mask = 0;
	if ( * ( char * ) xresources[ STOPMOUSEBUTTON ].var != '\0' )
	{
		if ( ! strcmp( ( char * ) xresources[ STOPMOUSEBUTTON ].var, "1" )
			 || ! strcasecmp( ( char * ) xresources[ STOPMOUSEBUTTON ].var,
							  "left" ) )
			GUI.stop_button_mask = FL_LEFT_MOUSE;
		else if ( ! strcmp( ( char * ) xresources[ STOPMOUSEBUTTON ].var,
							"2" ) ||
				  ! strcasecmp( ( char * ) xresources[ STOPMOUSEBUTTON ].var,
								"middle" ) )
			GUI.stop_button_mask = FL_MIDDLE_MOUSE;
		else if ( ! strcmp( ( char * ) xresources[ STOPMOUSEBUTTON ].var,
							"3" ) ||
				  ! strcasecmp( ( char * ) xresources[ STOPMOUSEBUTTON ].var,
								"right" ) )
			GUI.stop_button_mask = FL_RIGHT_MOUSE;
	}

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

	/* Set the HTTP port the server is going to run on - if it has been given
	   on the command line (or in XDefaults tec.) we take this value,
	   otherwise we use the compiled in value, but if this also doesn't exist
	   (or is also not within the range of non-privileged ports), we default
	   to the alternate HTTP port of 8080. */

	Internals.http_port = 8080;

	if ( * ( ( int * ) xresources[ HTTPPORT ].var ) >= 1024 &&
		 * ( ( int * ) xresources[ HTTPPORT ].var ) <= 65535 )
		Internals.http_port = * ( ( int * ) xresources[ HTTPPORT ].var );
#if defined DEFAULT_HTTP_PORT
	else if ( DEFAULT_HTTP_PORT >= 1024 && DEFAULT_HTTP_PORT <= 65535 )
		Internals.http_port = DEFAULT_HTTP_PORT;
#endif

	/* Load the functions for creating forms from the library dealing
	   with graphics */

	TRY
	{
		if ( ! dl_fsc2_rsc( ) )
			return FAIL;
		TRY_SUCCESS;
	}
	OTHERWISE
		return FAIL;

	/* Create and display the main form */

	GUI.main_form = GUI.G_Funcs.create_form_fsc2( );

	if ( ! ( Internals.cmdline_flags & NO_BALLOON ) )
	{
		fl_set_object_helper( GUI.main_form->Load, "Load new EDL program" );
		fl_set_object_helper( GUI.main_form->Edit, "Edit loaded EDL program" );
		fl_set_object_helper( GUI.main_form->reload, "Reload EDL program" );
		fl_set_object_helper( GUI.main_form->test_file, "Start syntax and "
							  "plausibility check" );
		fl_set_object_helper( GUI.main_form->run, "Start loaded EDL program" );
		fl_set_object_helper( GUI.main_form->quit, "Quit fsc2" );
		fl_set_object_helper( GUI.main_form->help, "Show documentation" );
		fl_set_object_helper( GUI.main_form->bug_report, "Mail a bug report" );
#if defined WITH_HTTP_SERVER
		www_help = get_string( "Run a HTTP server (on port %d)\n"
							   "that allows to view fsc2's current\n"
							   "state via the internet.",
							   Internals.http_port );
		fl_set_object_helper( GUI.main_form->server, www_help );
		T_free( www_help );
#endif
	}

	fl_set_browser_fontstyle( GUI.main_form->browser, FL_FIXED_STYLE );
	fl_set_browser_fontstyle( GUI.main_form->error_browser, FL_FIXED_STYLE );

	fl_get_object_geometry( GUI.main_form->browser, &cx1, &cy1, &cw1, &ch1 );
	fl_get_object_geometry( GUI.main_form->error_browser,
                            &cx2, &cy2, &cw2, &ch2 );

	h = cy2 - cy1 - ch1;
	H = ch1 + ch2 + h;

	fl_set_slider_size( GUI.main_form->win_slider, XI_sizes.SLIDER_SIZE );
	fl_set_slider_value( GUI.main_form->win_slider,
						 ( double ) ( ch1 + h / 2
									  - 0.5 * H * XI_sizes.SLIDER_SIZE )
						 / ( ( 1.0 - XI_sizes.SLIDER_SIZE ) * H ) );

	/* There's no use for the bug report button if either no mail address
	   or no mail program has been set */

#if ! defined( MAIL_ADDRESS ) || ! defined( MAIL_PROGRAM )
	fl_hide_object( GUI.main_form->bug_report );
#endif

	/* Don't draw a button for the HTTP server if it's not needed */

#if ! defined WITH_HTTP_SERVER
	fl_hide_object( GUI.main_form->server );
#endif

	/* Now show the form, taking user wishes about the geometry into account */

	if ( * ( ( char * ) xresources[ GEOMETRY ].var ) != '\0' )
	{
		flags = XParseGeometry( ( char * ) xresources[ GEOMETRY ].var,
								&wx, &wy, &ww, &wh );
		if ( WidthValue & flags && HeightValue & flags )
		{
			if ( ww < XI_sizes.WIN_MIN_WIDTH )
				ww = XI_sizes.WIN_MIN_WIDTH;
			if ( wh < XI_sizes.WIN_MIN_HEIGHT )
				wh = XI_sizes.WIN_MIN_HEIGHT;

			fl_set_form_size( GUI.main_form->fsc2, ww, wh );
		}

		if ( XValue & flags && YValue & flags )
			needs_pos = SET;
	}

	if ( needs_pos )
	{
		fl_set_form_position( GUI.main_form->fsc2, wx, wy );
		fl_show_form( GUI.main_form->fsc2, FL_PLACE_POSITION,
					  FL_FULLBORDER, "fsc2" );
	}
	else
		fl_show_form( GUI.main_form->fsc2, FL_PLACE_MOUSE | FL_FREE_SIZE,
					  FL_FULLBORDER, "fsc2" );

	fl_deactivate_object( GUI.main_form->reload );
	fl_set_object_lcol( GUI.main_form->reload, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->Edit );
	fl_set_object_lcol( GUI.main_form->Edit, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->test_file );
	fl_set_object_lcol( GUI.main_form->test_file, FL_INACTIVE_COL );
	fl_deactivate_object( GUI.main_form->run );
	fl_set_object_lcol( GUI.main_form->run, FL_INACTIVE_COL );

	XQueryTree( fl_display, GUI.main_form->fsc2->window, &root,
				&parent, &children, &nchilds );
	XQueryTree( fl_display, parent, &root,
				&parent, &children, &nchilds );
	XGetWindowAttributes( fl_display, parent, &attr );
	GUI.border_offset_x = GUI.main_form->fsc2->x - attr.x;
	GUI.border_offset_y = GUI.main_form->fsc2->y - attr.y;

	fl_winminsize( GUI.main_form->fsc2->window,
				   XI_sizes.WIN_MIN_WIDTH, XI_sizes.WIN_MIN_HEIGHT );

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

	fl_set_form_atclose( GUI.main_form->fsc2, main_form_close_handler,
                         NULL );

	/* Set c_cdata and u_cdata elements of load button structure */

	GUI.main_form->Load->u_ldata = 0;
	GUI.main_form->Load->u_cdata = NULL;

	/* Create the forms for writing a comment (both the comments to be stored
	   with the data and for printing) */

	GUI.input_form = GUI.G_Funcs.create_form_input_form( );
	GUI.print_comment = GUI.G_Funcs.create_pc_form( );

	/* Unset a flag that should only be set when the display window has been
	   drawn completely */

	G.is_fully_drawn = UNSET;

	return OK;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static void setup_app_options( FL_CMD_OPT app_opt[ ] )
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

	app_opt[ STOPMOUSEBUTTON ].option     = T_strdup( "-stopMouseButton" );
	app_opt[ STOPMOUSEBUTTON ].specifier  = T_strdup( "*.stopMouseButton" );
	app_opt[ STOPMOUSEBUTTON ].argKind    = XrmoptionSepArg;
	app_opt[ STOPMOUSEBUTTON ].value      = ( caddr_t ) "";

	app_opt[ NOCRASHMAIL ].option         = T_strdup( "-noCrashMail" );
	app_opt[ NOCRASHMAIL ].specifier      = T_strdup( "*.noCrashMail" );
	app_opt[ NOCRASHMAIL ].argKind        = XrmoptionNoArg;
	app_opt[ NOCRASHMAIL ].value          = ( caddr_t ) "0";

	app_opt[ RESOLUTION	].option          = T_strdup( "-size" );
	app_opt[ RESOLUTION	].specifier       = T_strdup( "*.size" );
	app_opt[ RESOLUTION	].argKind         = XrmoptionSepArg;
	app_opt[ RESOLUTION	].value           = ( caddr_t ) NULL;

	app_opt[ HTTPPORT ].option            = T_strdup( "-httpPort" );
	app_opt[ HTTPPORT ].specifier         = T_strdup( "*.httpPort" );
	app_opt[ HTTPPORT ].argKind           = XrmoptionSepArg;
	app_opt[ HTTPPORT ].value             = ( caddr_t ) "0";
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

bool dl_fsc2_rsc( void )
{
	char *lib_name = NULL;
	void *handle = NULL;
	char *ld_path;
	char *ld = NULL;
	char *ldc;


	/* Try to open the library with the stuff dealing with creating the forms.
	   We first try to find it in directories defined by the environment
	   variable "LD_LIBRARY_PATH". If this fails (and this is not part of the
	   testing procedure) we also try the compiled-in path to the libraries. */

	if ( ( ld_path = getenv( "LD_LIBRARY_PATH" ) ) != NULL )
	{
		ld = T_strdup( ld_path );
		for ( ldc = strtok( ld, ":" ); ldc != NULL; ldc = strtok( NULL, ":" ) )
		{
			lib_name = get_string( "%s%sfsc2_rsc_%cr.so", ldc, slash( ldc ),
								   GUI.G_Funcs.size == LOW ? 'l' : 'h' );
			if ( ( handle = dlopen( lib_name, RTLD_NOW ) ) != NULL )
				break;
			lib_name = T_free( lib_name );
		}
		T_free( ld );
	}

	if ( handle == NULL && ! ( Internals.cmdline_flags & DO_CHECK ) )
	{
		lib_name = get_string( "%s%sfsc2_rsc_%cr.so", libdir, slash( libdir ),
							   GUI.G_Funcs.size == LOW ? 'l' : 'h' );
		handle = dlopen( lib_name, RTLD_NOW );
	}

	if ( handle == NULL )
	{
		if ( lib_name == NULL )
			lib_name = get_string( "fsc2_rsc_%cr.so",
								   GUI.G_Funcs.size == LOW ? 'l' : 'h' );
		fprintf( stderr, "Can't open graphics library '%s'.\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	dlerror( );           /* make sure it's NULL before we continue */
	GUI.G_Funcs.create_form_fsc2 =
		       ( FD_fsc2 * ( * )( void ) ) dlsym( handle, "create_form_fsc2" );
	if ( dlerror( ) != NULL )
	{
		fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	dlerror( );           /* make sure it's NULL before we continue */
	GUI.G_Funcs.create_form_run =
		         ( FD_run * ( * )( void ) ) dlsym( handle, "create_form_run" );
	if ( dlerror( ) != NULL )
	{
		fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	dlerror( );           /* make sure it's NULL before we continue */
	GUI.G_Funcs.create_form_input_form = ( FD_input_form * ( * )( void ) )
		                             dlsym( handle, "create_form_input_form" );
	if ( dlerror( ) != NULL )
	{
		fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	dlerror( );           /* make sure it's NULL before we continue */
	GUI.G_Funcs.create_form_print =
		     ( FD_print * ( * )( void ) ) dlsym( handle, "create_form_print" );
	if ( dlerror( ) != NULL )
	{
		fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	dlerror( );           /* make sure it's NULL before we continue */
	GUI.G_Funcs.create_form_cut =
		         ( FD_cut * ( * )( void ) ) dlsym( handle, "create_form_cut" );
	if ( dlerror( ) != NULL )
	{
		fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	dlerror( );           /* make sure it's NULL before we continue */
	GUI.G_Funcs.create_pc_form =
		         ( FD_print_comment * ( * )( void ) ) dlsym( handle,
											     "create_form_print_comment" );
	if ( dlerror( ) != NULL )
	{
		fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
		T_free( lib_name );
		return FAIL;
	}

	T_free( lib_name );

	return OK;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

static int main_form_close_handler( FL_FORM *a, void *b )
{
	a = a;
	b = b;

	if ( GUI.main_form->quit->active != 1 )
		return FL_IGNORE;

	clean_up( );
	xforms_close( );

	exit( EXIT_SUCCESS );
	return FL_OK;                    /* keeps the compiler happy... */
}


/*-----------------------------------------*/
/* xforms_close() closes and deletes all   */
/* forms previously needed by the program. */
/*-----------------------------------------*/

void xforms_close( void )
{
	if ( fl_form_is_visible( GUI.main_form->fsc2 ) )
		fl_hide_form( GUI.main_form->fsc2 );
	fl_free_form( GUI.main_form->fsc2 );
}


/*------------------------------------------------------------*/
/* Callback function for movements of the slider that adjusts */
/* the sizes of the program and the error/output browser      */
/*------------------------------------------------------------*/

void win_slider_callback( FL_OBJECT *a, long b )
{
	FL_Coord h, H;
	FL_Coord new_h1 ;
	FL_Coord cx1, cy1, cw1, ch1, cx2, cy2, cw2, ch2;


	b = b;

	fl_freeze_form( GUI.main_form->fsc2 );

	fl_get_object_geometry( GUI.main_form->browser, &cx1, &cy1, &cw1, &ch1 );
	fl_get_object_geometry( GUI.main_form->error_browser,
                            &cx2, &cy2, &cw2, &ch2 );

	h = cy2 - cy1 - ch1;
	H = cy2 - cy1 + ch2;

	new_h1 = ( FL_Coord ) ( ( 1.0 - XI_sizes.SLIDER_SIZE ) * H
							* fl_get_slider_value( a )
							+ 0.5 * H * XI_sizes.SLIDER_SIZE - h / 2 );

	fl_set_object_size( GUI.main_form->browser, cw1, new_h1 );
	fl_set_object_geometry( GUI.main_form->error_browser, cx2,
                            cy1 + new_h1 + h, cw2, H - ( new_h1 + h ) );
	fl_unfreeze_form( GUI.main_form->fsc2 );
}

#if 0

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

#endif


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
