/*
 *  Copyright (C) 1999-2014 Jens Thoms Toerring
 *
 *  This file is part of fsc2.
 *
 *  Fsc2 is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  Fsc2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#pragma once
#if ! defined MODULE_UTIL_HEADER
#define MODULE_UTIL_HEADER


int get_mode( void );

int get_check_state( void );

int get_batch_state( void );

bool check_user_request( void );

void stop_on_user_request( void );

void too_many_arguments( Var_T * /* v */ );

void no_query_possible( void );

long get_long( Var_T      * restrict /* v       */,
               const char * restrict /* snippet */  );

double get_double( Var_T      * restrict /* v       */,
                   const char * restrict /* snippet */  );

long get_strict_long( Var_T      * restrict /* v       */,
                      const char * restrict /* snippet */  );

bool get_boolean( Var_T * /* v */ );

double is_mult_ns( double       /* val  */,
                   const char * /* text */  );

char *translate_escape_sequences( char * /* str */ );

double experiment_time( void );

FILE *fsc2_fopen( const char * restrict /* path */,
                  const char * restrict /* mode */  );

int fsc2_fscanf( FILE       * restrict /* stream */,
                 const char * restrict /* format */,
                 ...                        );

size_t fsc2_fread( void   * restrict /* ptr    */,
                   size_t            /* size   */,
                   size_t            /* nmemb  */,
                   FILE   * restrict /* stream */  );

int fsc2_fprintf( FILE       * restrict /* stream */,
                  const char * restrict /* format */,
                  ...                        );

size_t fsc2_fwrite( const void * restrict /* ptr    */,
                    size_t                /* size   */,
                    size_t                /* nmemb  */,
                    FILE       * restrict /* stream */  );

int fsc2_fgetc( FILE * /* stream */ );

int fsc2_getc( FILE * /* stream */ );

char *fsc2_fgets( char * restrict /* s      */,
                  int             /* size   */,
                  FILE * restrict /* stream */  );

int fsc2_ungetc( int    /* c      */,
                 FILE * /* stream */  );

int fsc2_fseek( FILE * /* stream */,
                long   /* offset */,
                int    /* whence */  );

long fsc2_ftell( FILE * /* stream */ );

int fsc2_fputc( int    /* c      */,
                FILE * /* stream */  );

int fsc2_fputs( const char * restrict /* s      */,
                FILE       * restrict /* stream */  );

int fsc2_putc( int    /* c      */,
               FILE * /* stream */  );

int fsc2_fclose( FILE * /* stream */ );

const char *fsc2_config_dir( void );

char * pretty_print( double                val,
                     char       * restrict buf,
                     char const * restrict unit );

#define PP_BUF_LEN  50
typedef char pp_buf[ PP_BUF_LEN + 1 ];

#define pp_s(  a, b )  pretty_print( a, b, "s" )
#define pp_sd( a, b )  pretty_print( a, b, "s/div" )
#define pp_v(  a, b )  pretty_print( a, b, "V" )
#define pp_vd( a, b )  pretty_print( a, b, "V/div" )
#define pp_a(  a, b )  pretty_print( a, b, "A" )
#define pp_o(  a, b )  pretty_print( a, b, "Ohm" )


#define MODULE_CALL_ESTIMATE   0.02   /* 20 ms per module function call -
                                         estimate for calculation of time in
                                         test run via experiment_time() */


#endif  /* ! MODULE_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
