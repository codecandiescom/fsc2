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


/* All of the following is to allow compiling also with a C++ compiler to
   get better type checking... */


#if ! defined FSC2_TYPES_HEADER
#define FSC2_TYPES_HEADER


#ifdef __cplusplus

#define inline

#define DOUBLE_P         ( double * )
#define LONG_P           ( long * )
#define INT_P            ( int * )
#define UINT_P           ( unsigned int * )
#define CHAR_P           ( char * )
#define UCHAR_P          ( unsigned char * )
#define CHAR_PP          ( char ** )
#define UCHAR_PP         ( unsigned char ** )
#define DEVICE_NAME_P    ( Device_Name * )
#define FUNC_P           ( Func * )
#define PULSER_STRUCT_P  ( Pulser_Struct * )
#define P_LIST_P         ( P_List * )
#define VAR_P            ( Var * )
#define CALL_STACK_P     ( CALL_STACK * )
#define DPOINT_P         ( DPoint * )
#define FILE_LIST_P      ( FILE_LIST * )
#define INPUT_RES_P      ( INPUT_RES * )
#define TOOL_BOX_P       ( TOOL_BOX * )
#define IOBJECT_P        ( IOBJECT * )
#define PHASE_SEQUENCE_P ( Phase_Sequence * )
#define DEVICE_P         ( Device * )
#define PRG_TOKEN_P      ( Prg_Token * )
#define CB_STACK_P       ( CB_Stack * )
#define SCALED_POINT_P   ( Scaled_Point * )
#define XPOINT_P         ( XPoint * )
#define CURVE_1D_P       ( Curve_1d * )
#define CURVE_2D_P       ( Curve_2d * )
#define MARKER_P         ( Marker * )
#define GRAPHICS_P       ( Graphics * )
#define G_HASH_ENTRY_P   ( G_Hash_Entry * )
#define PULSE_P          ( PULSE * )
#define PULSE_PP         ( PULSE ** )
#define DG2020_STORE_P   ( DG2020_STORE * )

#else

#define DOUBLE_P
#define LONG_P
#define INT_P
#define UINT_P
#define CHAR_P
#define UCHAR_P
#define CHAR_PP
#define UCHAR_PP
#define DEVICE_NAME_P
#define FUNC_P
#define PULSER_STRUCT_P
#define P_LIST_P
#define VAR_P
#define CALL_STACK_P
#define DPOINT_P
#define FILE_LIST_P
#define INPUT_RES_P
#define TOOL_BOX_P
#define IOBJECT_P
#define PHASE_SEQUENCE_P
#define DEVICE_P
#define PRG_TOKEN_P
#define CB_STACK_P
#define SCALED_POINT_P
#define XPOINT_P
#define CURVE_1D_P
#define CURVE_2D_P
#define MARKER_P
#define GRAPHICS_P
#define G_HASH_ENTRY_P
#define PULSE_P
#define PULSE_PP
#define DG2020_STORE_P

#endif

#endif  /* ! FSC2_TYPES_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * End:
 */
