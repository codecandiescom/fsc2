# $Id$
#
# Library for Rulbus (Rijksuniversiteit Leiden BUS)
#
# Copyright (C) 2001-2003 Jens Thoms Toerring
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# To contact the author send email to
# Jens.Toerring@physik.fu-berlin.de


CFLAGS    = -ggdb -O2 -Wall -Wwrite-strings -Wstrict-prototypes \
			-Wmissing-prototypes -Wmissing-declarations -W
LDFLAGS   = -Wl,-E
LEX       = flex
LFLAGS    = -B
YACC      = bison
YFLAGS    = -d -t
INCLUDES  = ../include

lib_sources   = rulbus_lib.c parser.c lexer.c rb8514.c rb8515.c
lib_headers   = $(INCLUDES)/rulbus.h


.SUFFIXES:                    # don't use implicit rules
.PHONY: all clean distclean install


all: librulbus.so librulbus.a

librulbus.so: ${lib_sources:.c=.o} $(lib_headers)
	$(CC) -shared -o $@ ${lib_sources:.c=.o} $(LDFLAGS)

librulbus.a: ${lib_sources:.c=.o} $(lib_headers)
	ar crs $@ ${lib_sources:.c=.o}

%.o: %.c $(lib_headers) parser.h
	$(CC) $(CFLAGS) -I$(INCLUDES) -c -o $@ $<

%.c %.h: %.y $(lib_headers)
	$(YACC) $(YFLAGS) -o $*.c $<

%.c: %.l $(lib_headers) parser.h
	$(LEX) $(LFLAGS) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@


install:
	install -d $(libdir)
	install -d $(incdir)
	install -m 644 librulbus.so $(libdir)
	install -m 644 librulbus.a $(libdir)
	install -m 644 rulbus.h $(incdir)


clean:
	$(RM) $(RMFLAGS) *.o lexer.c parser.c parser.h *~


distclean:
	$(MAKE) clean
	$(RM) $(RMFLAGS) *.a *.so


.PRECIOUS: %.c %.h
