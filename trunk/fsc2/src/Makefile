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
				vars_util.c func.c func_basic.c func_util.c func_save.c   \
                func_intact.c T.c phases.c exp.c devices.c run.c comm.c   \
                accept.c gpib_if.c chld_func.c loader.c conn.c ipc.c      \
                print.c graphics.c graph_handler_1d.c graph_handler_2d.c  \
                graph_cut.c bugs.c fsc2_assert.c dump.c serial.c module_util.c
bison_sources = $(wildcard *_parser.y)
flex_sources  = $(wildcard *_lexer.l)
sources       = $(bison_sources:.y=.c) $(flex_sources:.l=.c) $(c_sources)
objects       = $(sources:.c=.o)
resources     = $(wildcard fsc2_rsc_*r.fd)
headers       = ${c_sources:.c=.h} global.h
all_headers   = $(headers) $(bison_sources:.y=.h) $(resources:.fd=.h)

export headers        # export list of header files to the modules Makefile



.SUFFIXES:
.PHONY:    all modules res gpib_setup clean distclean


# Default target

all:
	$(MAKE) fsc2_rsc_lr.so fsc2_rsc_hr.so;
	$(MAKE) fsc2
	$(MAKE) fsc2_clean
	$(MAKE) fsc2_connect
	$(MAKE) modules;


# Make the main program from the c, flex and bison source files
# When linking all symbols must be added to the dynamic symbol table,
# the dynamically loded libraries need to see at least some of them.

fsc2: $(objects)
	$(CC) -o $@ $(objects) $(LIBS) -Wl,-E

modules:
	$(MAKE) -C ../modules all

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.c %.h: %.y $(headers)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) \
		-o $(addsuffix .c,$(basename $@)) $<


# The replacement of 'ECHO' by 'FLEX_ECHO' in the files created by flex is
# done because 'ECHO' (that is #defined in these files) is already defined
# in termios.h. The second replacement is for avoiding compiler warnings
# about unitialized variables in these files.

%.c: %.l $(headers)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.l,%,$<) -o$@ $<
	sed -e 's/\bECHO\b/FLEX_ECHO/g' $@ > $@.x
	sed -e 's/register char \*yy_cp, \*yy_bp;/\
		register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@.x > $@


# Make the primary lexer (which does the cleanup of the input EDL files)

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lm

fsc2_clean.c: fsc2_clean.l
	$(FLEX) -B -o$@ $<


# Make utility for interfacing with fsc2

fsc2_connect: fsc2_connect.o
	$(CC) -o $@ $^


# Create libraries for the graphical interface

%.so: %.o
	$(CC) $(LFLAGS) $^ -o $@
	chmod 644 $@

$(resources:.fd=.o): %.o: %.c %.h
	$(CC) $(CFLAGS) $(LFLAGS)  -c $< -o $@

%.c %.h: %.fd
	fdesign -convert $<


# Create files and links needed for the GPIB interface - they need making if
# either gpib_if.[ch] doesn't exit or the files they're made from are newer.
# When using the NI library also the parser and lexer for the GPIB
# configuration file are needed.

gpib_setup:
	@if [ -n "$(GPIB_LIBRARY)"                                       \
		  -a \(    "$(GPIB_LIBRARY)" = "old"                         \
			    -o "$(GPIB_LIBRARY)" = "ni"                          \
			    -o "$(GPIB_LIBRARY)" = "jtt"                         \
			    -o "$(GPIB_LIBRARY)" = "none" \) ]; then             \
		if [ ! gpib_if_$(GPIB_LIBRARY).cx -ef gpib_if.c ]; then      \
			$(RM) $(RMFLAGS) gpib_if.c;                              \
		fi;                                                          \
		if [ ! gpib_if_$(GPIB_LIBRARY).hx -ef gpib_if.h ]; then      \
			$(RM) $(RMFLAGS) gpib_if.h;                              \
		fi;                                                          \
		if [ ! -e gpib_if.c ]; then                                  \
			$(LN) $(LNFLAGS) gpib_if_$(GPIB_LIBRARY).cx gpib_if.c;   \
		fi;                                                          \
		if [ ! -e gpib_if.h ]; then                                  \
			$(LN) $(LNFLAGS) gpib_if_$(GPIB_LIBRARY).hx gpib_if.h;   \
		fi;                                                          \
		if [ "$(GPIB_LIBRARY)" = "ni" -a ! -e gpib_parser.y ]; then  \
			$(LN) $(LNFLAGS) gpib_ni_parser.yx gpib_parser.y;        \
		fi;                                                          \
		if [ "$(GPIB_LIBRARY)" = "ni" -a ! -e gpib_lexer.l ]; then   \
			$(LN) $(LNFLAGS) gpib_ni_lexer.lx gpib_lexer.l;          \
		fi;                                                          \
	else                                                             \
		echo "****************************************";             \
		echo "*   Make not correctly configured!     *";             \
		echo "****************************************";             \
		exit 2;                                                      \
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
	-$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_start
	-$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_test
	-$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_load


uninstall:
	-$(RM) $(RMFLAGS) $(bindir)/fsc2 $(bindir)/fsc2_clean          \
					  $(bindir)/fsc2_connect $(bindir)/fsc2_start  \
					  $(bindir)/fsc2_test $(bindir)/fsc2_load
	-for f in *.so; do                   \
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
