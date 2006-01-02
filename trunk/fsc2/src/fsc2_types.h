/*
 *  $Id$
 * 
 *  Copyright (C) 1999-2006 Jens Thoms Toerring
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


/* All of the following is to allow compiling also with a C++ compiler to
   get more strict type checking... */


#if ! defined FSC2_TYPES_HEADER
#define FSC2_TYPES_HEADER


#ifdef __cplusplus

#define inline

#define DOUBLE_P              ( double * )
#define DOUBLE_PP             ( double ** )
#define LONG_P                ( long * )
#define INT_P                 ( int * )
#define UINT_P                ( unsigned int * )
#define CHAR_P                ( char * )
#define UCHAR_P               ( unsigned char * )
#define CHAR_PP               ( char ** )
#define UCHAR_PP              ( unsigned char ** )
#define SSIZE_T_P             ( ssize_t * )
#define BOOL_P                ( bool * )
#define DEVICE_NAME_P         ( Device_Name_T * )
#define FUNC_P                ( Func_T * )
#define PULSER_STRUCT_P       ( Pulser_Struct_T * )
#define P_LIST_P              ( P_List_T * )
#define VAR_P                 ( Var_T * )
#define VAR_PP                ( Var_T ** )
#define CALL_STACK_P          ( Call_Stack_T * )
#define DPOINT_P              ( dpoint_T * )
#define FILE_LIST_P           ( File_List_T * )
#define INPUT_RES_P           ( Input_Res * )
#define TOOLBOX_P             ( Toolbox_T * )
#define IOBJECT_P             ( Iobject_T* )
#define PHASE_SETUP_P         ( Phase_Setup_T * )
#define PHS_SEQ_P             ( Phs_Seq_T * )
#define DEVICE_P              ( Device_T * )
#define PRG_TOKEN_P           ( Prg_Token_T * )
#define CB_STACK_P            ( CB_Stack_T * )
#define SCALED_POINT_P        ( Scaled_Point_T * )
#define XPOINT_P              ( XPoint * )
#define CURVE_1D_P            ( Curve_1d_T * )
#define CURVE_2D_P            ( Curve_2d_T * )
#define MARKER_1D_P           ( Marker_1d_T * )
#define MARKER_2D_P           ( Marker_2d_T * )
#define GRAPHICS_P            ( Graphics_T * )
#define GRAPHICS_1D_P         ( Graphics_1d_T * )
#define GRAPHICS_2D_P         ( Graphics_2d_T * )
#define G_HASH_ENTRY_P        ( G_Hash_Entry_T * )
#define PULSE_P               ( Pulse_T * )
#define PULSE_PP              ( Pulse_T ** )
#define PULSE_PPP             ( Pulse_T *** )
#define DG2020_STORE_P        ( dg2020_store_T * )
#define CHANNEL_PP            ( Channel_T ** )
#define WINDOW_P              ( Window_T * )
#define PULSE_PARAMS_P        ( Pulse_Params_T * )
#define ATT_TABLE_ENTRY_P     ( Att_Table_Entry_T * )
#define FS_P                  ( FS_T * )
#define UNS16_P               ( uns16 * )

#else

#define DOUBLE_P
#define DOUBLE_PP
#define LONG_P
#define INT_P
#define UINT_P
#define CHAR_P
#define UCHAR_P
#define CHAR_PP
#define UCHAR_PP
#define SSIZE_T_P
#define BOOL_P
#define DEVICE_NAME_P
#define FUNC_P
#define PULSER_STRUCT_P
#define P_LIST_P
#define VAR_P
#define VAR_PP
#define CALL_STACK_P
#define DPOINT_P
#define FILE_LIST_P
#define INPUT_RES_P
#define TOOLBOX_P
#define IOBJECT_P
#define PHASE_SETUP_P
#define PHS_SEQ_P
#define DEVICE_P
#define PRG_TOKEN_P
#define CB_STACK_P
#define SCALED_POINT_P
#define XPOINT_P
#define CURVE_1D_P
#define CURVE_2D_P
#define MARKER_1D_P
#define MARKER_2D_P
#define GRAPHICS_P
#define GRAPHICS_1D_P
#define GRAPHICS_2D_P
#define G_HASH_ENTRY_P
#define PULSE_P
#define PULSE_PP
#define PULSE_PPP
#define DG2020_STORE_P
#define CHANNEL_PP
#define WINDOW_P
#define PULSE_PARAMS_P
#define ATT_TABLE_ENTRY_P
#define FS_P
#define UNS16_P

#endif

#endif  /* ! FSC2_TYPES_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
