/*
 *  Copyright (C) 1999-2010 Jens Thoms Toerring
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


#include "fsc2.h"
#include <X11/Xresource.h>
#include <dlfcn.h>


static int main_form_close_handler( FL_FORM * a ,
                                    void *    b );
static void setup_app_options( FL_CMD_OPT app_opt[ ] );
static bool dl_fsc2_rsc( void );


/* Some variables needed for the X resources */

static FL_IOPT xcntl;

static struct {
    unsigned int WIN_MIN_WIDTH;
    unsigned int WIN_MIN_HEIGHT;
    int NORMAL_FONT_SIZE;
    int SMALL_FONT_SIZE;
    int TOOLS_FONT_SIZE;
    double SLIDER_SIZE;
} XI_Sizes;

static char xGeoStr[ 64 ],
            xdisplayGeoStr[ 64 ],
            x_1d_displayGeoStr[ 64 ],
            x_2d_displayGeoStr[ 64 ],
            xcutGeoStr[ 64 ],
            xtoolGeoStr[ 64 ],
            xaxisFont[ 256 ],
            xsmb[ 64 ],
            xsizeStr[ 64 ];

static int xbrowserfs,
           xtoolboxfs,
           xfileselectorfs,
           xhelpfs,
#if defined WITH_HTTP_SERVER
           xport,
#endif
		   xnocm;


FL_resource Xresources[ ] = {
    {                         /* geometry of main window */
        "geometry",
        "*.geometry",
        FL_STRING,
        xGeoStr,
        "",
        sizeof xGeoStr
    },
    {                         /* font size of the browsers in main window */
        "browserFontSize",
        "*.browserFontSize",
        FL_INT,
        &xbrowserfs,
        "0",
        sizeof xbrowserfs
    },
    {                         /* geometry of the display window */
        "displayGeometry",
        "*.displayGeometry",
        FL_STRING,
        xdisplayGeoStr,
        "",
        sizeof xdisplayGeoStr
    },
    {                         /* geometry of the 1D display window */
        "display1dGeometry",
        "*.display1dGeometry",
        FL_STRING,
        x_1d_displayGeoStr,
        "",
        sizeof x_1d_displayGeoStr
    },
    {                         /* geometry of the 2D display window */
        "display2dGeometry",
        "*.display2dGeometry",
        FL_STRING,
        x_2d_displayGeoStr,
        "",
        sizeof x_2d_displayGeoStr
    },
    {                         /* geometry of the cross section window */
        "cutGeometry",
        "*.cutGeometry",
        FL_STRING,
        xcutGeoStr,
        "",
        sizeof xcutGeoStr
    },
    {                         /* geometry of the tool box window */
        "toolGeometry",
        "*.toolGeometry",
        FL_STRING,
        xtoolGeoStr,
        "",
        sizeof xtoolGeoStr
    },
    {                         /* font for axes in the display windows */
        "axisFont",
        "*.axisFont",
        FL_STRING,
        xaxisFont,
        "",
        sizeof xaxisFont
    },
    {                         /* font for toolbox objects */
        "toolboxFontSize",
        "*.toolboxFontSize",
        FL_INT,
        &xtoolboxfs,
        "0",
        sizeof xtoolboxfs
    },
    {                         /* font for the file selector */
        "filselectorFontSize",
        "*.fileselectorFontSize",
        FL_INT,
        &xfileselectorfs,
        "0",
        sizeof xfileselectorfs
    },
    {                         /* font for the help texts */
        "helpFontSize",
        "*.helpFontSize",
        FL_INT,
        &xhelpfs,
        "0",
        sizeof xhelpfs
    },
    {                         /* mouse button to use to stop an experiment  */
        "stopMouseButton",
        "*.stopMouseButton",
        FL_STRING,
        xsmb,
        "",
        sizeof xsmb
    },
    {                         /* switch off crash mails */
        "noCrashMail",
        "*.noCrashMail",
        FL_BOOL,
        &xnocm,
        "0",
        sizeof xnocm
    },
    {                         /* selection of small or large version */
        "size",
        "*.size",
        FL_STRING,
        xsizeStr,
        "",
        sizeof xsizeStr
    },
#if defined WITH_HTTP_SERVER
    {                         /* number of port the HTTP server listens on */
        "httpPort",
        "*.httpPort",
        FL_INT,
        &xport,
        "0",
        sizeof xport
    }
#endif
};


/*--------------------------------------------------*
 * xforms_init() registers the program with XFORMS
 * and creates all the forms needed by the program.
 *--------------------------------------------------*/

bool
xforms_init( int  * argc,
             char * argv[ ] )
{
    Display *display;
    FL_Coord h, H;
    FL_Coord cx1, cy1, cw1, ch1, cx2, cy2, cw2, ch2;
    size_t i;
    int flags;
    XFontStruct *font;
    FL_CMD_OPT app_opt[ NUM_ELEMS( Xresources ) ];
#if defined WITH_HTTP_SERVER
    char *www_help;
#endif

    setup_app_options( app_opt );

    /* Overwrite the shells LANG environment variable - some versions of
       XForms use that variable and then mess around with the locale
       settings in unpleasant ways... */

    setenv( "LANG", "C", 1 );
    fl_set_border_width( -1 );

    if ( ( display = fl_initialize( argc, argv, "Fsc2", app_opt,
                                    NUM_ELEMS( Xresources ) ) ) == NULL )
        return FAIL;

    GUI.is_init = SET;
    GUI.d = display;

    /* Set the close-on-exec flag for the connection to the display */

    if ( ( flags = fcntl( ConnectionNumber( display ), F_GETFD, 0 ) ) >= 0 )
        fcntl( ConnectionNumber( display ), F_SETFD, flags | FD_CLOEXEC );

    /* We also must keep the main window from always becoming the topmost
       window at the most inconvenient moments... */

    fl_set_app_nomainform( 1 );

    /* All command line flags shold have been dealt with now. */

    if ( *argc > 1 && argv[ 1 ][ 0 ] == '-' )
    {
        fprintf( stderr, "Unknown option \"%s\".\n", argv[ 1 ] );
        usage( EXIT_FAILURE );
    }

    fl_get_app_resources( Xresources, NUM_ELEMS( Xresources ) );

    for ( i = 0; i < NUM_ELEMS( Xresources ); i++ )
    {
        T_free( app_opt[ i ].option );
        T_free( app_opt[ i ].specifier );
    }

    /* Find out the resolution we're going to run in */

    if ( fl_scrh >= 870 && fl_scrw >= 1152 )
        GUI.G_Funcs.size = ( bool ) HIGH;
    else
        GUI.G_Funcs.size = ( bool ) LOW;

    if ( * ( ( char * ) Xresources[ RESOLUTION ].var ) != '\0' )
    {
        if (    ! strcasecmp( ( char * ) Xresources[ RESOLUTION ].var, "s" )
             || ! strcasecmp( ( char * ) Xresources[ RESOLUTION ].var,
                              "small" ) )
            GUI.G_Funcs.size = ( bool ) LOW;
        if (    ! strcasecmp( ( char * ) Xresources[ RESOLUTION ].var, "l" )
             || ! strcasecmp( ( char * ) Xresources[ RESOLUTION ].var,
                              "large" ) )
            GUI.G_Funcs.size = ( bool ) HIGH;
    }

    if ( GUI.G_Funcs.size == LOW )
    {
        XI_Sizes.WIN_MIN_WIDTH    = 200;
        XI_Sizes.WIN_MIN_HEIGHT   = 320;
        XI_Sizes.NORMAL_FONT_SIZE = FL_SMALL_SIZE;
        XI_Sizes.SMALL_FONT_SIZE  = FL_TINY_SIZE;
        XI_Sizes.TOOLS_FONT_SIZE  = FL_SMALL_FONT;
        XI_Sizes.SLIDER_SIZE      = 0.2;
    }
    else
    {
        XI_Sizes.WIN_MIN_WIDTH    = 220;
        XI_Sizes.WIN_MIN_HEIGHT   = 570;
        XI_Sizes.NORMAL_FONT_SIZE = FL_MEDIUM_SIZE;
        XI_Sizes.SMALL_FONT_SIZE  = FL_SMALL_SIZE;
        XI_Sizes.TOOLS_FONT_SIZE  = FL_MEDIUM_FONT;
        XI_Sizes.SLIDER_SIZE      = 0.15;
    }

    /* Set some properties of goodies */

    fl_set_goodies_font( FL_NORMAL_STYLE, XI_Sizes.NORMAL_FONT_SIZE );
    fl_set_oneliner_font( FL_NORMAL_STYLE, XI_Sizes.NORMAL_FONT_SIZE );

    if ( * ( ( int * ) Xresources[ HELPFONTSIZE ].var ) != 0 )
        fl_set_tooltip_font( FL_NORMAL_STYLE,
                             * ( ( int * ) Xresources[ HELPFONTSIZE ].var ) );
    else
        fl_set_tooltip_font( FL_NORMAL_STYLE, XI_Sizes.SMALL_FONT_SIZE );

    if ( * ( ( int * ) Xresources[ FILESELFONTSIZE ].var ) != 0 )
        fl_set_fselector_fontsize( * ( ( int * )
                                       Xresources[ FILESELFONTSIZE ].var )  );
    else
        fl_set_fselector_fontsize( XI_Sizes.NORMAL_FONT_SIZE );
    fl_set_tooltip_color( FL_BLACK, FL_YELLOW );

    fl_disable_fselector_cache( 1 );
    fl_set_fselector_border( FL_TRANSIENT );

    /* Set default font sizes */

    xcntl.browserFontSize = XI_Sizes.NORMAL_FONT_SIZE;
    GUI.toolboxFontSize = XI_Sizes.TOOLS_FONT_SIZE;

    /* Set the stop mouse button (only used in the 'Display' window) */

    GUI.stop_button_mask = 0;
    if ( * ( char * ) Xresources[ STOPMOUSEBUTTON ].var != '\0' )
    {
        if (    ! strcmp( ( char * ) Xresources[ STOPMOUSEBUTTON ].var, "1" )
             || ! strcasecmp( ( char * ) Xresources[ STOPMOUSEBUTTON ].var,
                              "left" ) )
            GUI.stop_button_mask = FL_LEFT_MOUSE;
        else if (    ! strcmp( ( char * ) Xresources[ STOPMOUSEBUTTON ].var,
                               "2" )
                  || ! strcasecmp( ( char * )
                                   Xresources[ STOPMOUSEBUTTON ].var,
                                   "middle" ) )
            GUI.stop_button_mask = FL_MIDDLE_MOUSE;
        else if (    ! strcmp( ( char * ) Xresources[ STOPMOUSEBUTTON ].var,
                            "3" )
                  || ! strcasecmp( ( char * )
                                   Xresources[ STOPMOUSEBUTTON ].var,
                                   "right" ) )
            GUI.stop_button_mask = FL_RIGHT_MOUSE;
    }

    /* Set the default font size for browsers */

    if ( * ( ( int * ) Xresources[ BROWSERFONTSIZE ].var ) != 0 )
        xcntl.browserFontSize =
            * ( ( int * ) Xresources[ BROWSERFONTSIZE ].var );

    /* Set the default font size for the toolbox */

    if ( * ( ( int * ) Xresources[ TOOLBOXFONTSIZE ].var ) != 0 )
        GUI.toolboxFontSize =
            * ( ( int * ) Xresources[ TOOLBOXFONTSIZE ].var );

    /* Set the HTTP port the server is going to run on - if it has been given
       on the command line (or in .XDefaults etc.) we take this value,
       otherwise we use the compiled in value, but if this also doesn't exist
       (or is also not within the range of non-privileged ports), we default
       to the alternate HTTP port of 8080. */

    Fsc2_Internals.http_port = 8080;

#if defined DEFAULT_HTTP_PORT
    if (    * ( ( int * ) Xresources[ HTTPPORT ].var ) >= 1024
         && * ( ( int * ) Xresources[ HTTPPORT ].var ) <= 65535 )
        Fsc2_Internals.http_port = * ( ( int * ) Xresources[ HTTPPORT ].var );
    else if ( DEFAULT_HTTP_PORT >= 1024 && DEFAULT_HTTP_PORT <= 65535 )
        Fsc2_Internals.http_port = DEFAULT_HTTP_PORT;
#endif

    /* Load the library dealing for creating the graphics resources and
       resolve the functions for creating forms defined in it */

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

    if ( ! ( Fsc2_Internals.cmdline_flags & NO_BALLOON ) )
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
                               Fsc2_Internals.http_port );
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

    fl_set_slider_size( GUI.main_form->win_slider, XI_Sizes.SLIDER_SIZE );
    fl_set_slider_value( GUI.main_form->win_slider,
                         ( double ) ( ch1 + 0.5 * h
                                      - 0.5 * H * XI_Sizes.SLIDER_SIZE )
                         / ( ( 1.0 - XI_Sizes.SLIDER_SIZE ) * H ) );

    /* There's no use for the bug report button if either no mail address
       or no mail program has been set */

#if ! defined( MAIL_ADDRESS )
    fl_hide_object( GUI.main_form->bug_report );
#endif

    /* Don't draw a button for the HTTP server if it's not needed, otherwise
       add a callback for the server button */

#if defined WITH_HTTP_SERVER
    fl_set_object_callback( GUI.main_form->server, server_callback, 0 );
#else
    fl_hide_object( GUI.main_form->server );
#endif

    /* Now show the form, taking user wishes about the geometry into account */

    if ( * ( ( char * ) Xresources[ GEOMETRY ].var ) != '\0' )
    {
        flags = XParseGeometry( ( char * ) Xresources[ GEOMETRY ].var,
                                &GUI.win_x, &GUI.win_y,
                                &GUI.win_width, &GUI.win_height );

        GUI.win_has_pos  = XValue & flags     && YValue & flags;
        GUI.win_has_size = WidthValue & flags && HeightValue & flags;
    }

    if ( GUI.win_has_size )
    {
        if ( GUI.win_width < XI_Sizes.WIN_MIN_WIDTH )
            GUI.win_width = XI_Sizes.WIN_MIN_WIDTH;
        if ( GUI.win_height < XI_Sizes.WIN_MIN_HEIGHT )
            GUI.win_height = XI_Sizes.WIN_MIN_HEIGHT;

        fl_set_form_size( GUI.main_form->fsc2, GUI.win_width, GUI.win_height );
    }

    if ( GUI.win_has_pos )
        fl_set_form_position( GUI.main_form->fsc2, GUI.win_x, GUI.win_y );

    if ( ! ( Fsc2_Internals.cmdline_flags & ICONIFIED_RUN ) )
    {
        if ( GUI.win_has_pos )
            fl_show_form( GUI.main_form->fsc2, FL_PLACE_POSITION,
                          FL_FULLBORDER, "fsc2" );
        else
            fl_show_form( GUI.main_form->fsc2, FL_PLACE_CENTER | FL_FREE_SIZE,
                          FL_FULLBORDER, "fsc2" );
    }
    else
        fl_show_form( GUI.main_form->fsc2, FL_PLACE_ICONIC,
                      FL_FULLBORDER, "fsc2" );

    fl_deactivate_object( GUI.main_form->reload );
    fl_set_object_lcol( GUI.main_form->reload, FL_INACTIVE_COL );
    fl_deactivate_object( GUI.main_form->Edit );
    fl_set_object_lcol( GUI.main_form->Edit, FL_INACTIVE_COL );
    fl_deactivate_object( GUI.main_form->test_file );
    fl_set_object_lcol( GUI.main_form->test_file, FL_INACTIVE_COL );
    fl_deactivate_object( GUI.main_form->run );
    fl_set_object_lcol( GUI.main_form->run, FL_INACTIVE_COL );

    fl_winminsize( GUI.main_form->fsc2->window,
                   XI_Sizes.WIN_MIN_WIDTH, XI_Sizes.WIN_MIN_HEIGHT );

    /* Check if axis font exists (if the user set a font) */

    if ( * ( ( char * ) Xresources[ AXISFONT ].var ) != '\0' )
    {
        if ( ( font = XLoadQueryFont( display,
                                      ( char * ) Xresources[ AXISFONT ].var ) )
             == NULL )
            fprintf( stderr, "Error: Font '%s' requested for axes not "
                     "found.\n", ( char * ) Xresources[ AXISFONT ].var );
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

    /* Set a default directory for the file selector but only when it's not
       either obviously invalid (i.e. just an empty string, which shouldn't
       be possible), a stat() on it works, and if it's a directory and not
       a symbolic link (we better don't try to follow symbolic links, we could
       end up in a loop and it's not worth trying to implement a detection
       mechanism) or if not at least one of the permissions allows read access
       (we can't know here if the directory is going to be used for reading
       only or also writing, so we only check for the lowest hurdle). */

    if ( Fsc2_Internals.def_directory )
    {
        struct stat buf;

        if (    *Fsc2_Internals.def_directory
             && ! stat( Fsc2_Internals.def_directory, &buf ) < 0
             && S_ISDIR( buf.st_mode )
             && ! S_ISLNK( buf.st_mode )
             && buf.st_mode & ( S_IRUSR | S_IRGRP | S_IROTH ) )
            fl_set_directory( Fsc2_Internals.def_directory);

        Fsc2_Internals.def_directory = T_free( Fsc2_Internals.def_directory );
    }

    return OK;
}


/*-----------------------------------------------------------------------*
 * Setup the array used in handling the (remaining) command line options
 *-----------------------------------------------------------------------*/

static void
setup_app_options( FL_CMD_OPT app_opt[ ] )
{
    app_opt[ GEOMETRY ].option            = T_strdup( "-geometry" );
    app_opt[ GEOMETRY ].specifier         = T_strdup( "*.geometry" );
    app_opt[ GEOMETRY ].argKind           = XrmoptionSepArg;
    app_opt[ GEOMETRY ].value             = ( caddr_t ) NULL;

    app_opt[ BROWSERFONTSIZE ].option     =  T_strdup( "-browserFontSize" );
    app_opt[ BROWSERFONTSIZE ].specifier  = T_strdup( "*.browserFontSize" );
    app_opt[ BROWSERFONTSIZE ].argKind    = XrmoptionSepArg;
    app_opt[ BROWSERFONTSIZE ].value      = ( caddr_t ) "0";

    app_opt[ TOOLBOXFONTSIZE ].option     = T_strdup( "-toolboxFontSize" );
    app_opt[ TOOLBOXFONTSIZE ].specifier  = T_strdup( "*.toolboxFontSize" );
    app_opt[ TOOLBOXFONTSIZE ].argKind    = XrmoptionSepArg;
    app_opt[ TOOLBOXFONTSIZE ].value      = ( caddr_t ) "0";

    app_opt[ DISPLAYGEOMETRY ].option     = T_strdup( "-displayGeometry" );
    app_opt[ DISPLAYGEOMETRY ].specifier  = T_strdup( "*.displayGeometry" );
    app_opt[ DISPLAYGEOMETRY ].argKind    = XrmoptionSepArg;
    app_opt[ DISPLAYGEOMETRY ].value      = ( caddr_t ) NULL;

    app_opt[ DISPLAY1DGEOMETRY ].option   = T_strdup( "-display1dGeometry" );
    app_opt[ DISPLAY1DGEOMETRY ].specifier= T_strdup( "*.display1dGeometry" );
    app_opt[ DISPLAY1DGEOMETRY ].argKind  = XrmoptionSepArg;
    app_opt[ DISPLAY1DGEOMETRY ].value    = ( caddr_t ) NULL;

    app_opt[ DISPLAY2DGEOMETRY ].option   = T_strdup( "-display2dGeometry" );
    app_opt[ DISPLAY2DGEOMETRY ].specifier= T_strdup( "*.display2dGeometry" );
    app_opt[ DISPLAY2DGEOMETRY ].argKind  = XrmoptionSepArg;
    app_opt[ DISPLAY2DGEOMETRY ].value    = ( caddr_t ) NULL;

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

    app_opt[ RESOLUTION ].option          = T_strdup( "-size" );
    app_opt[ RESOLUTION ].specifier       = T_strdup( "*.size" );
    app_opt[ RESOLUTION ].argKind         = XrmoptionSepArg;
    app_opt[ RESOLUTION ].value           = ( caddr_t ) NULL;

#if defined DEFAULT_HTTP_PORT
    app_opt[ HTTPPORT ].option            = T_strdup( "-httpPort" );
    app_opt[ HTTPPORT ].specifier         = T_strdup( "*.httpPort" );
    app_opt[ HTTPPORT ].argKind           = XrmoptionSepArg;
    app_opt[ HTTPPORT ].value             = ( caddr_t ) "0";
#endif
}


/*--------------------------------------------------------------------*
 * Load the appropriate shared library for creating the graphic stuff
 * and try to resolve the addresses of the functions for creating the
 * various forms.
 *--------------------------------------------------------------------*/

bool
dl_fsc2_rsc( void )
{
    const char *lib_name;
    char *alt_lib_name;


    /* Try to open the library with the stuff dealing with creating the forms.
       Unless we are only supposed to use things from the sources directories
       we first try to find it in directories defined by the environment
       variable "LD_LIBRARY_PATH". If this fails (and this is not part of the
       testing procedure) we also try the compiled-in path to the libraries. */

    if ( GUI.G_Funcs.size == LOW )
    {
        if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
            lib_name = srcdir "fsc2_rsc_lr.fsc2_so";
        else
            lib_name = "fsc2_rsc_lr.fsc2_so";
    }
    else
    {
        if ( Fsc2_Internals.cmdline_flags & LOCAL_EXEC )
            lib_name = srcdir "fsc2_rsc_hr.fsc2_so";
        else
            lib_name = "fsc2_rsc_hr.fsc2_so";
    }

    Fsc2_Internals.rsc_handle = NULL;

    Fsc2_Internals.rsc_handle = dlopen( lib_name, RTLD_NOW );

    if (    Fsc2_Internals.rsc_handle == NULL
         && ! ( Fsc2_Internals.cmdline_flags & ( DO_CHECK | LOCAL_EXEC ) ) )
    {
        alt_lib_name = get_string( libdir "%s", lib_name );
        Fsc2_Internals.rsc_handle = dlopen( alt_lib_name, RTLD_NOW );
        T_free( alt_lib_name );
    }

    if ( Fsc2_Internals.rsc_handle == NULL )
    {
        fprintf( stderr, "Can't open graphics library '%s'.\n", lib_name );
        return FAIL;
    }

    /* Try to find the function for creating the main form */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_form_fsc2 = ( FD_fsc2 * ( * )( void ) )
                        dlsym( Fsc2_Internals.rsc_handle, "create_form_fsc2" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        return FAIL;
    }

    /* Try to find the function for creating the 1D display window */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_form_run_1d = ( FD_run_1d * ( * )( void ) )
                      dlsym( Fsc2_Internals.rsc_handle, "create_form_run_1d" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        dlclose( Fsc2_Internals.rsc_handle );
        Fsc2_Internals.rsc_handle = NULL;
        return FAIL;
    }

    /* Try to find the function for creating the 2D display window */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_form_run_2d = ( FD_run_2d * ( * )( void ) )
                      dlsym( Fsc2_Internals.rsc_handle, "create_form_run_2d" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        dlclose( Fsc2_Internals.rsc_handle );
        Fsc2_Internals.rsc_handle = NULL;
        return FAIL;
    }

    /* Try to find the function for creating the form for entering comments
       (for the EDL function ave_comment()) */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_form_input_form = ( FD_input_form * ( * )( void ) )
                  dlsym( Fsc2_Internals.rsc_handle, "create_form_input_form" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        dlclose( Fsc2_Internals.rsc_handle );
        Fsc2_Internals.rsc_handle = NULL;
        return FAIL;
    }

    /* Try to find the function for creating the form for printing during
       the experiment */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_form_print = ( FD_print * ( * )( void ) )
                       dlsym( Fsc2_Internals.rsc_handle, "create_form_print" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        dlclose( Fsc2_Internals.rsc_handle );
        Fsc2_Internals.rsc_handle = NULL;
        return FAIL;
    }

    /* Try to find the function for creating the cur section window */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_form_cut = ( FD_cut * ( * )( void ) )
                         dlsym( Fsc2_Internals.rsc_handle, "create_form_cut" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        dlclose( Fsc2_Internals.rsc_handle );
        Fsc2_Internals.rsc_handle = NULL;
        return FAIL;
    }

    /* Try to find the function for creating the form for entering comments
       to be include into the output when printing during the experiment */

    dlerror( );           /* make sure it's NULL before we continue */
    GUI.G_Funcs.create_pc_form = ( FD_print_comment * ( * )( void ) )
               dlsym( Fsc2_Internals.rsc_handle, "create_form_print_comment" );
    if ( dlerror( ) != NULL )
    {
        fprintf( stderr, "Error in graphics library '%s'\n", lib_name );
        dlclose( Fsc2_Internals.rsc_handle );
        Fsc2_Internals.rsc_handle = NULL;
        return FAIL;
    }

    return OK;
}


/*-----------------------------------------------------------------*
 * Handler for the user clicking on the Close button of the window
 * decoration - this is interpreted as the user really meaning it,
 * i.e. even when we're in batch mode and there are still EDL
 * scripts to be run quit immediately.
 *-----------------------------------------------------------------*/

static int
main_form_close_handler( FL_FORM * a  UNUSED_ARG,
                         void    * b  UNUSED_ARG )
{
    if ( GUI.main_form->quit->active != 1 )
        return FL_IGNORE;

    /* Do the same as if the "Quit" button has been hit (but quit immediately
       in batch mode and don't load the next EDL script) */

    clean_up( );
    xforms_close( );
    T_free( Fsc2_Internals.title );

    exit( EXIT_SUCCESS );
    return FL_OK;                    /* keeps the compiler happy... */
}


/*-----------------------------------------*
 * xforms_close() closes and deletes all
 * forms previously needed by the program.
 *-----------------------------------------*/

void
xforms_close( void )
{
    if ( fl_form_is_visible( GUI.main_form->fsc2 ) )
    {
        get_form_position( GUI.main_form->fsc2, &GUI.win_x, &GUI.win_y );
        GUI.win_width  = GUI.main_form->fsc2->w;
        GUI.win_height = GUI.main_form->fsc2->h;
        fl_hide_form( GUI.main_form->fsc2 );
    }

    fl_free_form( GUI.main_form->fsc2 );
    fl_free_form( GUI.input_form->input_form );
    fl_free_form( GUI.print_comment->print_comment );

    fl_finish( );

    fl_free( GUI.main_form );
    fl_free( GUI.input_form );
    fl_free( GUI.print_comment );

    /* Close the library for the graphics resources */

    if ( Fsc2_Internals.rsc_handle )
        dlclose( Fsc2_Internals.rsc_handle );
}


/*------------------------------------------------------------*
 * Callback function for movements of the slider that adjusts
 * the sizes of the program and the error/output browser.
 *------------------------------------------------------------*/

void
win_slider_callback( FL_OBJECT * a,
                     long        b  UNUSED_ARG )
{
    FL_Coord h, H;
    FL_Coord new_h1 ;
    FL_Coord cx1, cy1, cw1, ch1,
             cx2, cy2, cw2, ch2;
    int old_nwgravity, old_segravity;


    fl_freeze_form( GUI.main_form->fsc2 );

    fl_get_object_geometry( GUI.main_form->browser, &cx1, &cy1, &cw1, &ch1 );
    fl_get_object_geometry( GUI.main_form->error_browser,
                            &cx2, &cy2, &cw2, &ch2 );

    h = cy2 - cy1 - ch1;   /* height of strip between the browsers */
    H = cy2 - cy1 + ch2;   /* height from top of upper browser to bottom
                              of lower browser */

    new_h1 = lrnd( ( 1.0 - XI_Sizes.SLIDER_SIZE ) * H * fl_get_slider_value( a )
                   + 0.5 * ( H * XI_Sizes.SLIDER_SIZE - h ) );

    old_nwgravity = GUI.main_form->browser->nwgravity;
    old_segravity = GUI.main_form->browser->segravity;
    fl_set_object_gravity( GUI.main_form->browser,
                           old_nwgravity, ForgetGravity );

    fl_set_object_size( GUI.main_form->browser, cw1, new_h1 );

    fl_set_object_gravity( GUI.main_form->browser,
                           old_nwgravity, old_segravity );

    old_nwgravity = GUI.main_form->error_browser->nwgravity;
    old_segravity = GUI.main_form->error_browser->segravity;
    fl_set_object_gravity( GUI.main_form->error_browser,
                           ForgetGravity, old_segravity );

    fl_set_object_size( GUI.main_form->error_browser, cw2, H - ( new_h1 + h ) );

    fl_set_object_gravity( GUI.main_form->error_browser,
                           old_nwgravity, old_segravity );

    fl_unfreeze_form( GUI.main_form->fsc2 );
}


/*---------------------------------------------------------------------*
 * Function for figuring out if a window is iconified. It returns 1 if
 * it is, 0 if it isn't and -1 on errors. Please note that there's a
 * potential race condition when immedately after this test the state
 * of the window gets changed.
 *---------------------------------------------------------------------*/

#define WM_STATE_ELEMENTS 1

int
is_iconic( Display * d,
           Window    w )
{
    Atom xa_wm_state, actual_type;
    unsigned char *property = NULL;
    int actual_format;
    unsigned long nitems, leftover;
    int status;

    if ( ( xa_wm_state = XInternAtom( d, "WM_STATE", True ) ) == 0 )
        return -1;

    status = XGetWindowProperty( fl_get_display( ), w, xa_wm_state, 0,
                                 WM_STATE_ELEMENTS, False, xa_wm_state,
                                 &actual_type, &actual_format,
                                 &nitems, &leftover, &property );

    if (    status != Success
         || property == NULL
         || xa_wm_state != actual_type
         || nitems != 1 )
    {
        if ( property != NULL )
            XFree( property );
        return -1;
    }

    status = * ( unsigned long * ) property == IconicState;
    XFree(  property );
    return status;
}


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
