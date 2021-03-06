# Library for Rulbus (Rijksuniversiteit Leiden BUS)
#
# Copyright (C) 2003-2014 Jens Thoms Toerring
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
# To contact the author send email to jt@toerring.de


CFLAGS    = -ggdb -O2 -Wall -Wwrite-strings -Wstrict-prototypes \
			-Wmissing-prototypes -Wmissing-declarations -W -fpic
# LDFLAGS   = -Wl,--version-script=rulbus_lib.map
LEX       = flex
LFLAGS    = -B -P
YACC      = bison
YFLAGS    = -d -v -t -p

lib_sources   = rulbus_lib.c parser.c lexer.c rb8509.c rb8510.c \
				rb8514.c rb8515.c rb_generic.c
lib_headers   = rulbus.h rulbus_lib.h


.SUFFIXES:                    # don't use implicit rules
.PHONY: all cleanup clean install


all: librulbus.so librulbus.a

librulbus.so: ${lib_sources:.c=.o} $(lib_headers) rulbus_lib.map
	$(CC) -shared -o $@ ${lib_sources:.c=.o} $(LDFLAGS)

librulbus.a: ${lib_sources:.c=.o} $(lib_headers)
	ar crs $@ ${lib_sources:.c=.o}

%.o: %.c $(lib_headers) parser.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.c %.h: %.y $(lib_headers)
	$(YACC) $(YFLAGS) rulbus_ -o $*.c $<

%.c: %.l $(lib_headers) parser.h
	$(LEX) $(LFLAGS)rulbus_ -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/register char \*yy_cp = NULL, \*yy_bp = NULL;/;s/^find_rule:/\/\/find_rule:/' $@ > $@.x
	mv -f $@.x $@


install:
	install -d $(lib_dir)
	install -d $(incl_dir)
	install -m 755 librulbus.so $(lib_dir)
	install -m 644 librulbus.a $(lib_dir)
	install -m 644 rulbus.h $(incl_dir)


cleanup:
	$(RM) $(RMFLAGS) *.o lexer.c parser.c parser.h  parser.output *~


clean:
	$(MAKE) cleanup
	$(RM) $(RMFLAGS) *.a *.so


.PRECIOUS: %.c %.h
