#  $Id$
#
#  Copyright (C) 1999-2006 Jens Thoms Toerring
#
#  This file is part of fsc2.
#
#  Fsc2 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  Fsc2 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with fsc2; see the file COPYING.  If not, write to
#  the Free Software Foundation, 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.


# Installatiion of the various state / configuration /calibration / table
# etc. files that are needed for some of the instruments. Care is taken to
# extract the name of the required file from the configuration file for
# the device and to avoid overwriting an already file in the destination
# directory.

install:
ifdef WITH_RULBUS
	rb_pulser_cards=`grep '^[ \t]*#define[ \t]\{1,\}RB_PULSER_CONFIG_FILE' rb_pulser.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}RB_PULSER_CONFIG_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                      \
	if [      -n "$$rb_pulser_cards"                                 \
		 -a   -e $$rb_pulser_cards                                   \
		 -a ! -e $(libdir)/$$rb_pulser_cards ]; then                 \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$rb_pulser_cards  \
												  $(libdir);         \
	fi
endif

ifdef WITH_RS_SPEC10
	rs_spec10_state=`grep '^[ \t]*#define[ \t]\{1,\}RS_SPEC10_STATE_FILE' rs_spec10.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}RS_SPEC10_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                        \
	if [      -n "$$rs_spec10_state"                                 \
		 -a   -e $$rs_spec10_state                                   \
		 -a ! -e $(libdir)/$$rs_spec10_state ]; then                 \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$rs_spec10_state  \
												  $(libdir);         \
	fi
endif

ifdef WITH_WITIO_48
	hjs_fc_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' hjs_fc.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                                          \
	if [      -n "$$hjs_fc_calib"                                            \
		 -a   -e $$hjs_fc_calib                                              \
		 -a ! -e $(libdir)/$$hjs_fc_calib ]; then                            \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$hjs_fc_calib $(libdir);  \
	fi
endif

	spex_cd2a_state=`grep '^[ \t]*#define[ \t]\{1,\}SPEX_CD2A_STATE_FILE' spex_cd2a.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}SPEX_CD2A_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                        \
	if [      -n "$$spex_cd2a_state"                                 \
		 -a   -e $$spex_cd2a_state                                   \
		 -a ! -e $(libdir)/$$spex_cd2a_state ]; then                 \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$spex_cd2a_state  \
												  $(libdir);         \
	fi

	hp8647a_table=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE' hp8647a.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                                         \
	if [      -n "$$hp8647a_table"                                            \
		 -a   -e $$hp8647a_table                                              \
		 -a ! -e $(libdir)/$$hp8647a_table ]; then                            \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$hp8647a_table $(libdir);  \
	fi

	hp8648b_table=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE' hp8648b.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                                   \
	if [      -n "$$hp8648b_table"                                            \
		 -a   -e $$hp8648b_table                                              \
		 -a ! -e $(libdir)/$$hp8648b_table ]; then                            \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$hp8648b_table $(libdir);  \
	fi

	hjs_attenuator_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' hjs_attenuator.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                 \
	if [      -n "$$hjs_attenuator_calib"                                 \
		 -a   -e $$hjs_attenuator_calib                                   \
		 -a ! -e $(libdir)/$$hjs_attenuator_calib ]; then                 \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$hjs_attenuator_calib  \
                                                  $(libdir);              \
	fi

	spectrapro_300i_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' spectrapro_300i.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                \
	if [      -n "$$spectrapro_300i_calib"                                 \
		 -a   -e $$spectrapro_300i_calib                                   \
		 -a ! -e $(libdir)/$$spectrapro_300i_calib ]; then                 \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$spectrapro_300i_calib  \
                                                  $(libdir);               \
	fi

	hjs_sfc_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' hjs_sfc.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                                 \
	if [      -n "$$hjs_sfc_calib"                                            \
		 -a   -e $$hjs_sfc_calib                                              \
		 -a ! -e $(libdir)/$$hjs_sfc_calib ]; then                            \
		$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $$hjs_sfc_calib $(libdir);  \
	fi


uninstall:
ifdef WITH_RULBUS
	rb_pulser_cards=`grep '^[ \t]*#define[ \t]\{1,\}RB_PULSER_CONFIG_FILE' rb_pulser.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}RB_PULSER_CONFIG_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                        \
	if [ -n "$$rb_pulser_cards" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$rb_pulser_cards;  \
	fi
endif

ifdef WITH_RS_SPEC10
	rs_spec10_state=`grep '^[ \t]*#define[ \t]\{1,\}RS_SPEC10_STATE_FILE' rs_spec10.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}RS_SPEC10_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                          \
	if [ -n "$$rs_spec10_state" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$rs_spec10_state;  \
	fi
endif
ifdef WITH_WITIO_48
	hjs_fc_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' hjs_fc.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                 \
	if [ -n "$$hjs_fc_calib" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$hjs_fc_calib;  \
	fi
endif
	spex_cd2a_state=`grep '^[ \t]*#define[ \t]\{1,\}SPEX_CD2A_STATE_FILE' spex_cd2a.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}SPEX_CD2A_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                          \
	if [ -n "$$spex_cd2a_state" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$spex_cd2a_state;  \
	fi

	hp8647a_table=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE' hp8647a.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                \
	if [ -n "$$hp8647a_table" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$hp8647a_table;  \
	fi

	hp8648b_table=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE' hp8648b.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_TABLE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                                \
	if [ -n "$$hp8648b_table" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$hp8648b_table;  \
	fi

	hjs_attenuator_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' hjs_attenuator.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                   \
	if [ -n "$$hjs_attenuator_calib" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$hjs_attenuator_calib;  \
	fi

	spectrapro_300i_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' spectrapro_300i.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                  \
	if [ -n "$$spectrapro_300i_calib" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$spectrapro_300i_calib;  \
	fi

	hjs_sfc_calib=`grep '^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_FILE' hjs_sfc.conf 2>/dev/null | sed 's/^[ \t]*#define[ \t]\{1,\}DEFAULT_CALIB_STATE_FILE[ \t]\{1,\}"\([^"]\{1,\}\)"/\1/'`;                          \
	if [ -n "$$hjs_sfc_calib" ]; then                \
		$(RM) $(RMFLAGS) $(libdir)/$$hjs_sfc_calib;  \
	fi
