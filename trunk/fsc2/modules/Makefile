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


simp_modules   = User_Functions.c sr510.c sr530.c sr810.c sr830.c \
				 aeg_s_band.c aeg_x_band.c er035m.c er035m_s.c \
				 er035m_sa.c er035m_sas.c bh15.c keithley228a.c egg4402.c \
				 kontron4060.c lakeshore330.c pt2025.c
dg2020_f       = dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b       = dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
hfs9000        = hfs9000.c hfs9000_gen.c hfs9000_pulse.c hfs9000_util.c \
				 hfs9000_init.c hfs9000_run.c hfs9000_gpib.c
tds754a        = tds754a.c tds754a_gpib.c tds754a_util.c
tds744a        = tds744a.c tds744a_gpib.c tds744a_util.c
tds540         = tds540.c  tds540_gpib.c  tds540_util.c
tds520c        = tds520c.c tds520c_gpib.c tds520c_util.c
tds520a        = tds520a.c tds520a_gpib.c tds520a_util.c
tds520         = tds520.c  tds520_gpib.c  tds520_util.c
hp8647a        = hp8647a.c hp8647a_util.c hp8647a_gpib.c hp8647a_lexer.c
comp_modules   = dg2020_f dg2020_b hfs9000 tds754a tds744a tds540 tds520c \
                 tds520a tds520 hp8647a

src_headers    = $(addprefix ../src/,$(headers))

CFLAGS        += -I../src
LFLAGS         = -shared -fpic 


.SUFFIXES:
.PHONY: all clean


all: $(comp_modules:=.so) $(simp_modules:.c=.so)


dg2020_f.so: ${dg2020_f:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

dg2020_b.so: ${dg2020_b:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

hfs9000.so: ${hfs9000:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

tds754a.so: ${tds754a:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

tds744a.so: ${tds744a:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

tds540.so: ${tds540:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

tds520c.so: ${tds520c:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

tds520a.so: ${tds520a:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

tds520.so: ${tds520:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

hp8647a.so: ${hp8647a:.c=.o}
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

%.so: %.o
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@

${dg2020_f:.c=.o}: dg2020_f.h
${dg2020_b:.c=.o}: dg2020_b.h
${hfs9000:.c=.o}:  hfs9000.h
${tds754a:.c=.o}:  tds754a.h
${tds744a:.c=.o}:  tds744a.h
${tds540:.c=.o}:   tds540.h
${tds520c:.c=.o}:  tds520c.h
${tds520a:.c=.o}:  tds520a.h
${tds520:.c=.o}:   tds520.h
${hp8647a:.c=.o}:  hp8647a.h

hp8647a_lexer.c: hp8647a_lexer.l $(src_header)
	$(FLEX) $(FLEXFLAGS)$(patsubst %lexer.l,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/\
			register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@

%.o: %.c $(src_headers)
	$(CC) $(CFLAGS) -c $(LFLAGS) $< -o $@


clean:
	-$(RM) $(RMFLAGS) *.o *_lexer.c *.so TAGS *~


# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
