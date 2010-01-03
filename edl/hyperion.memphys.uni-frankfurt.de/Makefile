#  Copyright (C) 1999-2010 Jens Thoms Toerring
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


EDL         := $(wildcard *.EDL)
EDL_SCRIPTS := $(patsubst %.EDL,%,$(EDL))
SCRIPTS     := $(EDL_SCRIPTS) epr fcntrl2 fsps25_shutdown field_mode \
			   fsps25_shutdown_zero monitor_mode pfail_recover


all: $(EDL_SCRIPTS)

%: %.EDL
	../../utils/fsc2_guify $< $@

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
