/*
 *  Copyright (C) 1999-2009 Jens Thoms Toerring
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


#if ! defined FUNC_UTIL_HEADER
#define FUNC_UTIL_HEADER


#include "fsc2.h"


Var_T * f_print(           Var_T * /* v */ );
Var_T * f_sprint(          Var_T * /* v */ );
Var_T * f_showm(           Var_T * /* v */ );
Var_T * f_wait(            Var_T * /* v */ );
Var_T * f_init_1d(         Var_T * /* v */ );
Var_T * f_init_2d(         Var_T * /* v */ );
Var_T * f_dmode(           Var_T * /* v */ );
Var_T * f_cscale(          Var_T * /* v */ );
Var_T * f_cscale_1d(       Var_T * /* v */ );
Var_T * f_cscale_2d(       Var_T * /* v */ );
Var_T * f_vrescale(        Var_T * /* v */ );
Var_T * f_vrescale_1d(     Var_T * /* v */ );
Var_T * f_vrescale_2d(     Var_T * /* v */ );
Var_T * f_clabel(          Var_T * /* v */ );
Var_T * f_clabel_1d(       Var_T * /* v */ );
Var_T * f_clabel_2d(       Var_T * /* v */ );
Var_T * f_rescale(         Var_T * /* v */ );
Var_T * f_rescale_1d(      Var_T * /* v */ );
Var_T * f_rescale_2d(      Var_T * /* v */ );
Var_T * f_display(         Var_T * /* v */ );
Var_T * f_display_1d(      Var_T * /* v */ );
Var_T * f_display_2d(      Var_T * /* v */ );
Var_T * f_clearcv(         Var_T * /* v */ );
Var_T * f_clearcv_1d(      Var_T * /* v */ );
Var_T * f_clearcv_2d(      Var_T * /* v */ );
Var_T * f_setmark(         Var_T * /* v */ );
Var_T * f_setmark_1d(      Var_T * /* v */ );
Var_T * f_setmark_2d(      Var_T * /* v */ );
Var_T * f_clearmark(       Var_T * /* v */ );
Var_T * f_clearmark_1d(    Var_T * /* v */ );
Var_T * f_clearmark_2d(    Var_T * /* v */ );
Var_T * f_get_pos(         Var_T * /* v */ );
Var_T * f_curve_button(    Var_T * /* v */ );
Var_T * f_curve_button_1d( Var_T * /* v */ );
Var_T * f_curve_button_2d( Var_T * /* v */ );
Var_T * f_fs_button(       Var_T * /* v */ );
Var_T * f_fs_button_1d(    Var_T * /* v */ );
Var_T * f_fs_button_2d(    Var_T * /* v */ );
Var_T * f_zoom(            Var_T * /* v */ );
Var_T * f_zoom_1d(         Var_T * /* v */ );
Var_T * f_zoom_2d(         Var_T * /* v */ );
Var_T * f_find_peak(       Var_T * /* v */ );
Var_T * f_index_of_max(    Var_T * /* v */ );
Var_T * f_index_of_min(    Var_T * /* v */ );
Var_T * f_mean_part_array( Var_T * /* v */ );
Var_T * f_spike_rem(       Var_T * /* v */ );


#endif  /* ! FUNC_UTIL_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
