# $Id$
#
# Copyright (C) 2001 Jens Thoms Toerring
#
# This file is part of fsc2.
#
# Fsc2 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# Fsc2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with fsc2; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


c_sources     = fsc2.c xinit.c pulser.c util.c exceptions.c variables.c   \
				vars_util.c func.c func_basic.c func_util.c func_intact.c \
				T.c phases.c exp.c devices.c run.c gpib_if.c chld_func.c  \
				graphics.c comm.c accept.c graph_handler_1d.c             \
				graph_handler_2d.c graph_cut.c loader.c bugs.c print.c    \
				conn.c ipc.c fsc2_assert.c dump.c serial.c
bison_sources = $(wildcard *_parser.y)
flex_sources  = $(wildcard *_lexer.l)
sources       = $(bison_sources:.y=.c) $(flex_sources:.l=.c) ${c_sources}
objects       = $(sources:.c=.o)
headers       = ${c_sources:.c=.h} global.h

export headers        # export list of header files to the modules Makefile

SLFLAGS        = -shared -fpic


.SUFFIXES:
.PHONY:    all modules res gpib clean distclean


# Default target - make only GPIB_LIBRARY is set correctly

all:
	@if [ -n "$(GPIB_LIBRARY)"                            \
		  -a \(    "$(GPIB_LIBRARY)" = "old"              \
			    -o "$(GPIB_LIBRARY)" = "ni"               \
			    -o "$(GPIB_LIBRARY)" = "jtt"              \
			    -o "$(GPIB_LIBRARY)" = "none" \) ]; then  \
		$(MAKE) fsc2_rsc_lr.so fsc2_rsc_hr.so gpib;       \
		$(MAKE) fsc2 fsc2_clean fsc2_connect modules;     \
	else                                                  \
		echo "****************************************";  \
		echo "*   Make not correctly configured!     *";  \
		echo "****************************************";  \
		exit 2;                                           \
	fi


# Make the main program from the c, flex and bison source files

fsc2: $(objects)
	$(CC) -o $@ $(objects) $(LIBS) -rdynamic

modules:
	$(MAKE) -C ../modules all

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y $(headers)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) -o $@ $<

%.c: %.l $(headers)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.l,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/\
		register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@


# Make the primary lexer that's utilized by the main program

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lfl -lm

fsc2_clean.c: fsc2_clean.l $(headers)
	$(FLEX) -B -o$@ $<


# Make utilities for interfacing with fsc2

fsc2_connect: fsc2_connect.o
	$(CC) -o $@ $^


# Create files and links needed for for graphical interface - they need making
# if either fsc2_rsc.[ch] or fsc2_rsc_[hl]r.[ch] don't exist, the file they
# are made from is newer or the existing files are for the wrong resolution

%.so: %.o
	$(LD) $(SLFLAGS) $^ -o $@
	chmod 644 $@

fsc2_rsc_lr.o: fsc2_rsc_lr.c
	$(CC) $(CFLAGS) -c $(SLFLAGS) $< -o $@

fsc2_rsc_hr.o: fsc2_rsc_hr.c
	$(CC) $(CFLAGS) -c $(SLFLAGS) $< -o $@

fsc2_rsc_lr.c: fsc2_rsc_lr.fd
	fdesign -convert fsc2_rsc_lr.fd;                             \

fsc2_rsc_hr.c: fsc2_rsc_hr.fd
	fdesign -convert fsc2_rsc_hr.fd;                             \


# Create files and links needed for the GPIB interface - they need making if
# either gpib_if.[ch] doesn't exit or the files they're made from are newer.
# When using the NI library also the parser and lexer for the GPIB
# configuration file are needed.

gpib:
	@if [ ! -e gpib_if.c -o ! -e gpib_if.h                     \
		  -o gpib_if_$(GPIB_LIBRARY).c -nt gpib_if.c           \
		  -o gpib_if_$(GPIB_LIBRARY).h -nt gpib_if.h           \
		  -o gpib_if_$(GPIB_LIBRARY).c -ef gpib_if.c           \
		  -o gpib_if_$(GPIB_LIBRARY).h -ef gpib_if.h ]; then   \
		$(RM) $(RMFLAGS) gpib_if.[ch];                         \
		$(LN) $(LNFLAGS) gpib_if_$(GPIB_LIBRARY).c gpib_if.c;  \
		$(LN) $(LNFLAGS) gpib_if_$(GPIB_LIBRARY).h gpib_if.h;  \
	fi;                                                        \
	if [ "$(GPIB_LIBRARY)" = "ni" ]; then                      \
		if [ ! -e gpib_lexer.l                                 \
	          -o gpib_lexer.ll -nt gpib_lexer.l                \
	          -o gpib_lexer.ll -ed gpib_lexer.l ]; then        \
			$(RM) $(RMFLAGS) gpib_lexer.l;                     \
			$(LN) $(LNFLAGS) gpib_lexer.ll gpib_lexer.l;       \
		fi;                                                    \
		if [ ! -e gpib_lexer.l                                 \
	    	  -o gpib_lexer.ll -nt gpib_lexer.l                \
	      	  -o gpib_lexer.ll -ed gpib_lexer.l ]; then        \
			$(RM) $(RMFLAGS) gpib_lexer.l;                     \
			$(LN) $(LNFLAGS) gpib_lexer.ll gpib_lexer.l;       \
		fi;                                                    \
	fi


# How to install the binaries

install:
	$(INSTALL) -d $(bindir)
	$(INSTALL) -d $(libdir)

	$(EXEC_INSTALL) -m 6755 -o $(OWNER) -g $(GROUP) fsc2 $(bindir)
	$(EXEC_INSTALL) -m  755 -o $(OWNER) -g $(GROUP) fsc2_clean $(bindir)
	$(EXEC_INSTALL) -m  755 -o $(OWNER) -g $(GROUP) fsc2_connect $(bindir)
	$(EXEC_INSTALL) -m  644 -o $(OWNER) -g $(GROUP) *.so $(libdir)

	-$(RM) $(RMFLAGS) $(bindir)/fsc2_start $(bindir)/fsc2_test \
					  $(bindir)/fsc2_load
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_start
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_test
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_load


uninstall:
	-$(RM) $(RMFLAGS) $(bindir)/fsc2 $(bindir)/fsc2_clean
					  $(bindir)/fsc2_connect $(bindir)/fsc2_start
					  $(bindir)/fsc2_test $(bindir)/fsc2_load
	for f in *.so; do                    \
		$(RM) $(RMFLAGS) $(libdir)/$$f;  \
	done

# How to get rid of stuff not needed anymore

clean:
	-$(RM) $(RMFLAGS) *.o *.output *_parser.[ch] *_lexer.c fsc2_clean.c  \
					  gpib_if.[ch] gpib_parser.y gpib_lexer.l mem cscope.out *~

distclean:
	$(MAKE) clean
	-$(RM) $(RMFLAGS) fsc2 fsc2_clean fsc2_connect fsc2_rsc_[hl]r.[ch] *.so


# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c

