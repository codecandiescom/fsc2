#  Copyright (C) 1999-2014 Jens Thoms Toerring
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


c_sources      = fsc2.c xinit.c pulser.c util.c exceptions.c variables.c     \
				 vars_rhs.c vars_lhs.c vars_ass.c vars_add.c vars_sub.c      \
				 vars_mult.c vars_div.c vars_mod.c vars_pow.c vars_util.c    \
				 func.c func_basic.c func_util.c func_save.c chld_func.c     \
				 func_intact.c func_intact_b.c func_intact_s.c               \
				 func_intact_o.c func_intact_m.c T.c phases.c devices.c      \
				 exp.c run.c comm.c accept.c gpib.c loader.c ipc.c locks.c   \
				 print.c serial.c lan.c graphics.c graphics_edl.c    \
				 graph_handler_1d.c graph_handler_2d.c graph_cut.c bugs.c    \
				 fsc2_assert.c dump.c module_util.c global.c help.c  \
				 edit.c 

ifdef WITH_HTTP_SERVER
c_sources     += http.c dump_graphic.c
endif

bison_sources  = $(patsubst gpib_%.y,,$(wildcard *_parser.y))
flex_sources   = $(patsubst gpib_%.l,,$(wildcard *_lexer.l))
sources        = $(bison_sources:.y=.c) $(flex_sources:.l=.c) $(c_sources)
objects        = $(sources:.c=.o)
resources      = $(wildcard fsc2_rsc_*r.fd)
headers        = ${c_sources:.c=.h} ../fsc2_config.h
all_headers    = $(headers) $(bison_sources:.y=.h) $(resources:.fd=.h)

export headers        # export list of header files to the modules Makefile


.SUFFIXES:
.PHONY:    all src res gpib_setup xforms_setup cleanup clean


# Default target which is used when make is called from within this directory

all:
	$(MAKE) -C .. src


# Everything not belonging to the modules

src:
	$(MAKE) fsc2_rsc_lr.fsc2_so fsc2_rsc_hr.fsc2_so
	$(MAKE) fsc2
	$(MAKE) fsc2_clean
	$(MAKE) fsc2_connect
ifneq ($(GPIB_LIBRARY),none)
	$(MAKE) gpibd
endif
ifdef ADDR2LINE
	$(MAKE) fsc2_addr2line
endif


modules:
	$(MAKE) -C ../modules modules


# Make the main program from the C, flex and bison source files
# When linking all symbols must be added to the dynamic symbol table,
# the dynamically loaded libraries need to see at least some of them.

fsc2: $(objects)
	$(CC) -o $@ $^ $(LIBS) -Wl,-E

%.o: %.c $(headers)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

%.c %.h: %.y $(headers)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) \
			 -o $(addsuffix .c,$(basename $<)) $<


# The replacement of 'ECHO' by 'FLEX_ECHO' in the files created by flex is
# done because 'ECHO' (which is #defined in these files) is already also
# defined in termios.h. The second replacement is for avoiding a warning (at
# least for newer versions of flex) about a comparison betwen a signed and
# an unsigned int .

%.c: %.l $(headers)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.l,%,$<) -o$@ $<
	sed -e 's/\bECHO\b/FLEX_ECHO/g' \
		-e 's/for ( n = 0; n < max_size && \\$$/for ( n = 0; ( size_t ) n < max_size \&\& \\/' $@ > $@.x
	mv $@.x $@


# Make the primary lexer (which does the cleanup of the input EDL files)

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lm


fsc2_clean.c: fsc2_clean.l
	$(FLEX) -B -o$@ $<
	sed 's/for ( n = 0; n < max_size && \\$$/for ( n = 0; ( size_t ) n < max_size \&\& \\/' $@ > $@.x
	mv $@.x $@


# Make utility for interfacing with fsc2

fsc2_connect: fsc2_connect.o
	$(CC) -o $@ $<


# Make utility for interfacing with fsc2

fsc2_addr2line: fsc2_addr2line.o
	$(CC) -o $@ $<


# Create libraries for the graphical interface

%.fsc2_so: %.o
	$(CC) $(LFLAGS) -o $@ $<
	chmod 644 $@

$(resources:.fd=.o): %.o: %.c %.h
	$(CC) $(CFLAGS) -fPIC $(INCLUDES) -c -o $@ $<

%.c %.h: %.fd
	fdesign -convert $<


# Make the GPIB daemon

gpibd: gpibd.c gpib_if.c $(GPIB_HELPER) gpibd.h gpib_if.h ../fsc2_config.h
	$(CC) $(CFLAGS) $(INCLUDES) -o gpibd gpibd.c gpib_if.c $(GPIB_HELPER)  \
		  -lpthread $(GPIB_LIBS)

# Create files and links needed for the GPIB interface - they need making if
# either gpib_if.[ch] doesn't exit or the files they're made from are newer.
# When using the NI library also the parser and lexer for the GPIB
# configuration file are needed, otherwise they must be deleted.

gpib_setup:
ifneq ($(GPIB_LIBRARY),none)
	@if [ -n "$(GPIB_LIBRARY)"                                       \
		  -a \(    "$(GPIB_LIBRARY)" = "llp"                         \
			    -o "$(GPIB_LIBRARY)" = "ni"                          \
			    -o "$(GPIB_LIBRARY)" = "ni_old"                      \
			    -o "$(GPIB_LIBRARY)" = "slg"                         \
			    -o "$(GPIB_LIBRARY)" = "jtt"  \) ]; then             \
		if [  ! gpib_if_$(GPIB_LIBRARY).cx -ef gpib_if.c             \
			 -o gpib_if_$(GPIB_LIBRARY).cx -nt gpib_if.c ]; then     \
			$(RM) $(RMFLAGS) gpib_if.c;                              \
		fi;                                                          \
		if [  ! gpib_if_$(GPIB_LIBRARY).hx -ef gpib_if.h             \
			 -o gpib_if_$(GPIB_LIBRARY).hx -nt gpib_if.h ]; then     \
			$(RM) $(RMFLAGS) gpib_if.h;                              \
		fi;                                                          \
		if [ ! -e gpib_if.c ]; then                                  \
			$(LN) $(LNFLAGS) gpib_if_$(GPIB_LIBRARY).cx gpib_if.c;   \
		fi;                                                          \
		if [ ! -e gpib_if.h ]; then                                  \
			$(LN) $(LNFLAGS) gpib_if_$(GPIB_LIBRARY).hx gpib_if.h;   \
		fi;                                                          \
		if [    "$(GPIB_LIBRARY)" = "ni"                             \
			 -o "$(GPIB_LIBRARY)" = "ni_old" ]; then                 \
			if [ ! -e gpib_parser.y ]; then                          \
				$(LN) $(LNFLAGS) gpib_ni_parser.yx gpib_parser.y;    \
			fi;                                                      \
			if [ ! -e gpib_lexer.l ]; then                           \
				$(LN) $(LNFLAGS) gpib_ni_lexer.lx gpib_lexer.l;      \
			fi;                                                      \
		else                                                         \
			$(RM) $(RMFLAGS) gpib_parser.y gpib_lexer.l;             \
		fi;                                                          \
	else                                                             \
		echo "************************************************";     \
		echo "*   GPIB settings not correctly configured!    *";     \
		echo "************************************************";     \
		exit 2;                                                      \
	fi
endif

# How to install the binaries

install:
	$(INSTALL) -d $(bindir)
	$(INSTALL) -d $(libdir)

ifdef ONLY_SETGID
ifneq ($(GPIB_LIBRARY),none)
	$(INSTALL) -m 2755 -o $(OWNER) -g $(GROUP) gpibd $(bindir)
endif
	$(INSTALL) -m 2755 -o $(OWNER) -g $(GROUP) fsc2 $(bindir)
else
ifdef ONLY_SETUID
ifneq ($(GPIB_LIBRARY),none)
	$(INSTALL) -m 4755 -o $(OWNER) -g $(GROUP) gpibd $(bindir)
endif
	$(INSTALL) -m 4755 -o $(OWNER) -g $(GROUP) fsc2 $(bindir)
else
ifneq ($(GPIB_LIBRARY),none)
	$(INSTALL) -m 6755 -o $(OWNER) -g $(GROUP) gpibd $(bindir)
endif
	$(INSTALL) -m 6755 -o $(OWNER) -g $(GROUP) fsc2 $(bindir)
endif
endif
	$(INSTALL) -m  755 -o $(OWNER) -g $(GROUP) fsc2_clean $(bindir)
	$(INSTALL) -m  755 -o $(OWNER) -g $(GROUP) fsc2_connect $(bindir)
ifdef ADDR2LINE
	$(INSTALL) -m  755 -o $(OWNER) -g $(GROUP) fsc2_addr2line $(bindir)
endif
	$(INSTALL) -m  644 -o $(OWNER) -g $(GROUP) *.fsc2_so $(libdir)

	$(RM) $(RMFLAGS) $(bindir)/fsc2_start $(bindir)/fsc2_test        \
	                 $(bindir)/fsc2_nw    $(bindir)/fsc2_load        \
                     $(bindir)/fsc2_iconic_start
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_start
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_test
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_nw
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_load
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_iconic_start

uninstall:
	$(RM) $(RMFLAGS) $(bindir)/fsc2 $(bindir)/fsc2_clean                  \
					 $(bindir)/fsc2_connect $(bindir)/fsc2_start          \
					 $(bindir)/fsc2_connect $(bindir)/fsc2_nw             \
					 $(bindir)/fsc2_connect $(bindir)/fsc2_iconic_start   \
					 $(bindir)/fsc2_test $(bindir)/fsc2_load
ifneq ($(GPIB_LIBRARY),none)
	$(RM) $(RMFLAGS) $(bindir)/gpibd
endif
ifdef ADDR2LINE
	$(RM) $(RMFLAGS) $(bindir)/fsc2_addr2line
endif
	for f in *.fsc2_so; do                                                \
		$(RM) $(RMFLAGS) $(libdir)/$$f;                                   \
	done


# How to get rid of stuff not needed anymore

cleanup:
	$(RM) $(RMFLAGS) *.o *.output *_parser.[ch] *_lexer.c fsc2_clean.c       \
		             gpib_if.[ch] gpib_parser.y gpib_lexer.l mem cscope.out  \
	                 *~

clean:
	$(MAKE) cleanup
	$(RM) $(RMFLAGS) fsc2 gpibd fsc2_clean fsc2_connect fsc2_rsc_[hl]r.[ch]  \
		  fsc2_addr2line *.fsc2_so *.bak


# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
