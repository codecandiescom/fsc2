/*
  $Id$
*/


#include "fsc2.h"


#define INCH   25.4                    /* mm in an inch */

static double paper_width = 210.0;
static double paper_height = 297.0;

static int get_print_file( FILE **fp, char **name );
static void print_header( FILE *fp )




void print_1d( void )
{
	int print_type;
	FILE *fp;
	char *name;


	/* Find out about the way to print and get a file, then print the header */

   if ( ( print_type = get_print_file( &fp, &name ) ) == 0 )
	   return;
	print_header( fp, name );

	T_free( name );
}


void print_2d( void )
{
	int print_type;
	FILE *fp;
	char *name;


	/* Find out about the way to print and get a file, then print the header */

	if ( ( print_type = get_print_file( &fp, &name ) ) == 0 )
		return;
	print_header( fp, name );

	T_free( name );
}


int get_print_file( FILE **fp, char **name )
{
	if ( ( *fp = fopen( "xyz.eps", "w" ) ) == NULL )
		return;
	*name = get_string_copy( "xyz.eps" );
	return 1;
}


void print_header( FILE *fp, char *name )
{
	int i;
	FILE *df;
	int key;


	/* Start writing header into the file */

	fprintf( fp, "%%!PS-Adobe-3.0 EPSF-3.0\n"
			     "%%%%Creator: fsc2, December 1999 - JTT\n"
			     "%%%%CreationDate: " );

	/* Write result of call of `date' into the file */

	if ( ( df = popen( "date", "r" ) ) != NULL )
	{
		while ( ( key = fgetc( df ) ) != EOF )
			fputc( key, fp );
		pclose( df );
	}
	else
		fputc( '\n', fp );

	/* Continue with rest of header */

	fprintf( fp, "%%%%Title: %s\n"
			     "%%%%BoundingBox: 0 0 %d %d\n"
			     "%%%%End Comments\n",
			     name,
			     ( int ) ( 72.0 * paper_width / INCH ),
			     ( int ) ( 72.0 * paper_height / INCH ) );

	/* Some (hopefully) helpful definitions */

	fprintf( fp, "%%%%BeginProlog\n"
			     "/s { stroke } bind def\n"
                 "/t { translate } bind def\n"
			     "/r { rotate} bind def\n"
			     "/cp { closepath } bind def\n"
				 "/m { moveto } bind def\n"
			     "/l { lineto } bind def\n"
			     "/rm { rmoveto } bind def\n"
			     "/rl { rlineto } bind def\n"
			     "/slw { setlinewidth } bind def\n"
			     "/srgb { setrgbcolor } bind def\n"
			     "/sd { setdash } bind def\n"
			     "/sw { stringwidth pop } bind def\n"
				 "/sf { exch findfont exch scalefont setfont } bind def\n"
			     "/gs { gsave } bind def\n"
			     "/gr { grestore } bind def\n"
			     "/slc { setlinecap } bind def\n"
			     "/ch { gs newpath 0 0 moveto\n"
			     "      false charpath flattenpath pathbbox\n"
                 "      exch pop 3 -1 roll pop exch pop gr } bind def\n"
				 "/cw { gs newpath 0 0 moveto\n"
	             "      false charpath flattenpath pathbbox\n"
	             "      pop exch pop exch pop gr } bind def\n"
                 "/esw { dup sw exch dup length 1 sub 1 getinterval\n"
                 "       dup cw exch sw sub add } bind def\n"
			     "/yoff { (X) ch 2 div } bind def\n"
			     "/rs { dup 2 index lt { pop }{ exch pop } ifelse } bind def\n"
				 "/rect { gs 3 1 roll t\n"
				 "        dup neg dup neg dup dup neg dup neg dup neg dup\n"
				 "        m l l l cp s gr } bind def\n"
			     "/circ { 0 360 arc cp } bind def\n"
			     "/ell { gs 5 -2 roll t 3 -1 roll r\n"
                 "       0.5 3 -1 roll scale\n"
                 "       0 0 3 -1 roll circ fill gr } bind def\n"
			     "%%%%EndProlog\n" );

	/* Rotate and shift for landscape format */

	fprintf( fp, "90 r\n, %d 0 t\n", ( int ) ( 72.0 * paper_height / INCH ) );
}
