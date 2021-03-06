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
#if ! defined COMM_HEADER
#define COMM_HEADER


#include "fsc2.h"


/* The size of the message queue - the higher it is the longer the child
   process can continue without having to wait for the parent. But, on the
   other hand, setting it to something much larger that the maximum number
   of shared memory segments won't help... */

#define QUEUE_SIZE 256


enum {
    C_EPRINT = 0,
    C_SHOW_MESSAGE,
    C_SHOW_ALERT,
    C_ACK,
    C_SHOW_CHOICES,
    C_SHOW_FSELECTOR,
    C_PROG,
    C_OUTPUT,
    C_INPUT,
    C_STR,
    C_INT,
    C_LONG,
    C_FLOAT,
    C_DOUBLE,
    C_LAYOUT,
    C_LAYOUT_REPLY,
    C_BCREATE,
    C_BCREATE_REPLY,
    C_BDELETE,
    C_BDELETE_REPLY,
    C_BSTATE,
    C_BSTATE_REPLY,
    C_BCHANGED,
    C_BCHANGED_REPLY,
    C_SCREATE,
    C_SCREATE_REPLY,
    C_SDELETE,
    C_SDELETE_REPLY,
    C_SSTATE,
    C_SSTATE_REPLY,
    C_SCHANGED,
    C_SCHANGED_REPLY,
    C_ICREATE,
    C_ICREATE_REPLY,
    C_IDELETE,
    C_IDELETE_REPLY,
    C_ISTATE,
    C_ISTATE_REPLY,
    C_ISTATE_STR_REPLY,
    C_ICHANGED,
    C_ICHANGED_REPLY,
    C_MCREATE,
    C_MCREATE_REPLY,
    C_MADD,
    C_MADD_REPLY,
    C_MTEXT,
    C_MTEXT_REPLY,
    C_MDELETE,
    C_MDELETE_REPLY,
    C_MCHOICE,
    C_MCHOICE_REPLY,
    C_MCHANGED,
    C_MCHANGED_REPLY,
    C_TBCHANGED,
    C_TBCHANGED_REPLY,
    C_TBWAIT,
    C_TBWAIT_REPLY,
    C_ODELETE,
    C_ODELETE_REPLY,
    C_FREEZE,
    C_CLABEL,
    C_CLABEL_REPLY,
    C_XABLE,
    C_XABLE_REPLY,
    C_GETPOS,
    C_GETPOS_REPLY,
    C_CB_1D,
    C_CB_1D_REPLY,
    C_CB_2D,
    C_CB_2D_REPLY,
    C_ZOOM_1D,
    C_ZOOM_1D_REPLY,
    C_ZOOM_2D,
    C_ZOOM_2D_REPLY,
    C_FSB_1D,
    C_FSB_1D_REPLY,
    C_FSB_2D,
    C_FSB_2D_REPLY
};


enum {
    D_CLEAR_CURVE   = -1,
    D_CHANGE_SCALE  = -2,
    D_CHANGE_LABEL  = -3,
    D_CHANGE_POINTS = -4,
    D_SET_MARKER    = -5,
    D_CLEAR_MARKERS = -6,
    D_CHANGE_MODE   = -7,
    D_VERT_RESCALE  = -8
};


typedef struct Comm_Struct Comm_Struct_T;
typedef struct Slot Slot_T;
typedef struct Message_Queue Message_Queue_T;


struct Comm_Struct {
    int type;
    union {
        ssize_t len;
        int int_data;
        long long_data;
        float float_data;
        double double_data;
        ssize_t str_len[ 4 ];
    } data;
};


struct Slot {
    int type;
    int shm_id;
};


struct Message_Queue {
    int low;
    int high;
    Slot_T slot[ QUEUE_SIZE ];
};


enum {
    DATA_1D = 1,
    DATA_2D = 2,
    REQUEST = 4
};


void setup_comm( void );

void end_comm( void );

int new_data_handler( void );

bool reader( void * /* ret */ );

bool writer( int /* type */,
             ...             );

void send_data( int /* type   */,
                int /* shm_id */  );


#endif  /* ! COMM_HEADER */


/*
 * Local variables:
 * tags-file-name: "../TAGS"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
