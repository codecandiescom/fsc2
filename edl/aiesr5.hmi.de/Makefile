#  Copyright (C) 1999-2011 Jens Thoms Toerring
#
#  This file is part of fsc2.
#
#  Fsc2 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3, or (at your option)
#  any later version.
#
#  Fsc2 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.


EDL         := $(wildcard *.EDL)
EDL_SCRIPTS := $(patsubst %.EDL,%,$(EDL))
SCRIPTS     := $(EDL_SCRIPTS) set_temperature self_tune heater_off epr


all: $(EDL_SCRIPTS)

%: %.EDL
	../../utils/fsc2_guify $< $@

set_res:
	./set_resonators ER023M monitor_bruker.EDL.orig > monitor_bruker.EDL
	./set_resonators ER023M cw_epr_bruker.EDL.orig > cw_epr_bruker.EDL
	./set_resonators SR830 monitor_sr.EDL.orig > monitor_sr.EDL
	./set_resonators SR830 cw_epr_sr.EDL.orig > cw_epr_sr.EDL

install:
	for f in $(SCRIPTS); do                \
		$(INSTALL) -m 0755 $$f $(bindir);  \
	done

uninstall:
	for f in $(SCRIPTS); do                \
		$(RM) $(RMFLAGS) $(bindir)/$$f;    \
	done

clean:
	$(RM) $(RMFLAGS) $(EDL_SCRIPTS) *~
