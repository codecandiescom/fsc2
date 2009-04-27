#  Copyright (C) 1999-2009 Jens Thoms Toerring
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


EDL      := $(wildcard *.EDL)
scripts  := $(patsubst %.EDL,%,$(EDL))

all: $(scripts)

%: %.EDL
	../../utils/fsc2_guify $< $@

install:
	@for f in `ls`; do                                      \
		if [ -x $f ]; then                                  \
			echo "cp $$f $(bindir)/$$f.$(machine_name)";    \
	    	cp $$f $(bindir)/$$f.$(machine_name);           \
		fi                                                  \
	done

uninstall:
	@for f in `ls`; do                                              \
		if [    -x $$f                                              \
			 -a -f $(bindir)/$$f                                    \
			 -a ! diff $$f $(bindir)/$$f > /dev/null 2>&1 ]; then   \
			$(RM) $(RMFLAGS) $(bindir)/$$f;                         \
		fi                                                          \
	done

clean:
	$(RM) $(RMFLAGS) $(patsubst %.EDL,%,$(EDL)) *~
