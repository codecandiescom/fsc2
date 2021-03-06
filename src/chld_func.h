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
#if ! defined CHLD_FUNC_HEADER
#define CHLD_FUNC_HEADER


#include "fsc2.h"


typedef struct Input_Res Input_Res_T;

struct Input_Res {
    int res;
    union {
        long lval;
        double dval;
        char *sptr;
    } val;
};


void show_message( const char * /* str */ );

void show_alert( const char * /* str */ );

int show_choices( const char * /* text     */,
                  int          /* numb     */,
                  const char * /* b1       */,
                  const char * /* b2       */,
                  const char * /* b3       */,
                  int          /* def      */,
                  bool         /* is_batch */  );

const char *show_fselector( const char * /* message   */,
                            const char * /* directory */,
                            const char * /* pattern   */,
                            const char * /* def       */  );

const char *show_input( const char * /* content */,
                        const char * /* label   */  );

bool exp_layout( char      * /* buffer */,
                 ptrdiff_t   /* len    */  );

long *exp_bcreate( char      * /* buffer */,
                   ptrdiff_t   /* len    */  );

bool exp_bdelete( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

long *exp_bstate( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

long *exp_bchanged( char      * /* buffer */,
                    ptrdiff_t   /* len    */  );

long *exp_screate( char      * /* buffer */,
                   ptrdiff_t   /* len    */  );

bool exp_sdelete( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

double *exp_sstate( char      * /* buffer */,
                    ptrdiff_t   /* len    */  );

long *exp_schanged( char      * /* buffer */,
                    ptrdiff_t   /* len    */  );

long *exp_icreate( char      * /* buffer */,
                   ptrdiff_t   /* len    */  );

bool exp_idelete( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

Input_Res_T *exp_istate( char      * /* buffer */,
                         ptrdiff_t   /* len    */  );

long *exp_ichanged( char      * /* buffer */,
                    ptrdiff_t   /* len    */  );

long *exp_mcreate( char      * /* buffer */,
                   ptrdiff_t   /* len    */  );

bool exp_madd( char      * /* buffer */,
               ptrdiff_t   /* len    */ );

char * exp_mtext( char      * /* buffer */,
                  ptrdiff_t   /* len    */ );

bool exp_mdelete( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

long *exp_mchoice( char      * /* buffer */,
                   ptrdiff_t   /* len    */  );

long *exp_mchanged( char      * /* buffer */,
                    ptrdiff_t   /* len    */  );

long *exp_tbchanged( char      * /* buffer */,
                     ptrdiff_t   /* len    */  );

long *exp_tbwait( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

bool exp_objdel( char      * /* buffer */,
                 ptrdiff_t   /* len    */  );

bool exp_clabel( char      * /* buffer */,
                 ptrdiff_t   /* len    */  );

bool exp_xable( char      * /* buffer */,
                ptrdiff_t   /* len    */  );

double *exp_getpos( char      * /* buffer */,
                    ptrdiff_t   /* len    */  );

bool exp_cb_1d( char      * /* buffer */,
                ptrdiff_t   /* len    */  );

bool exp_cb_2d( char      * /* buffer */,
                ptrdiff_t   /* len    */  );

bool exp_zoom_1d( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

bool exp_zoom_2d( char      * /* buffer */,
                  ptrdiff_t   /* len    */  );

bool exp_fsb_1d( char      * /* buffer */,
                 ptrdiff_t   /* len    */  );

bool exp_fsb_2d( char      * /* buffer */,
                 ptrdiff_t   /* len    */  );


#endif  /* ! CHLD_FUNC_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
