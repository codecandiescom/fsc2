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

src_headers    = $(addprefix $(sdir)/,$(headers))

CFLAGS        += -I$(sdir) -I$(cdir)
LFLAGS         = -shared -fpic

define libmake
	$(LD) $(LFLAGS) $^ -o $@
	chmod 644 $@
endef


.SUFFIXES:
.PHONY: all clean distclean


all: $(addsuffix .so,$(comp_modules)) $(simp_modules:.c=.so)


dg2020_f.so: $(dg2020_f:.c=.o)
	$(libmake)

dg2020_b.so: $(dg2020_b:.c=.o)
	$(libmake)

hfs9000.so: $(hfs9000:.c=.o)
	$(libmake)

tds754a.so: $(tds754a:.c=.o)
	$(libmake)

tds744a.so: $(tds744a:.c=.o)
	$(libmake)

tds540.so: $(tds540:.c=.o)
	$(libmake)

tds520c.so: $(tds520c:.c=.o)
	$(libmake)

tds520a.so: $(tds520a:.c=.o)
	$(libmake)

tds520.so: $(tds520:.c=.o)
	$(libmake)

hp8647a.so: $(hp8647a:.c=.o)
	$(libmake)


$(dg2020_f:.c=.o): %.o: dg2020_f.h $(cdir)/dg2020_f.conf
$(dg2020_b:.c=.o): %.o: dg2020_b.h $(cdir)/dg2020_b.conf
$(hfs9000:.c=.o):  %.o: hfs9000.h  $(cdir)/hfs9000.conf
$(tds754a:.c=.o):  %.o: tds754a.h  $(cdir)/tds754a.conf
$(tds744a:.c=.o):  %.o: tds744a.h  $(cdir)/tds744a.conf
$(tds540:.c=.o):   %.o: tds540.h   $(cdir)/tds540.conf
$(tds520c:.c=.o):  %.o: tds520c.h  $(cdir)/tds520c.conf
$(tds520a:.c=.o):  %.o: tds520a.h  $(cdir)/tds520a.conf
$(tds520:.c=.o):   %.o: tds520.h   $(cdir)/tds520.conf
$(hp8647a:.c=.o):  %.o: hp8647a.h  $(cdir)/hp8647a.conf


%.o: %.c $(src_headers)
	$(CC) -c $(CFLAGS) $(LFLAGS) $< -o $@

hp8647a_lexer.c: hp8647a_lexer.l $(src_header)
	$(FLEX) $(FLEXFLAGS)$(patsubst %lexer.l,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/\
			register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@

$(simp_modules:.c=.o): %.o: %.c $(cdir)/%.conf $(src_headers)
	$(CC) $(CFLAGS) -c $(LFLAGS) $< -o $@

%.so: %.o
	$(libmake)


# How to install the modules

install:
	$(INSTALL) -d $(libdir)
	$(EXEC_INSTALL) -m  644 -o $(OWNER) -g $(GROUP) *.so $(libdir)
	if [ "$(INSTALL_MACHINE)" != "install-unknown" ]; then  \
		$(MAKE) $(INSTALL_MACHINE);                         \
	fi


# Here links to module libraries for often used devices for the different
# spectrometers are created. If, for example, the spectrometer always uses
# aeg_x_band.so as the driver for the magnet power supply, creating a link
# with the name magnet.so allows to use 'magnet' as an alternative name for
# the power supply in EDL programs. Don't forget to delete already existing
# links before creating new ones!

install-pion.anorg.chemie.uni-frankfurt.de:
	-$(RM) $(RMFLAGS) $(libdir)/dg2020.so
	$(LN) $(LNFLAGS)  $(libdir)/dg2020_f.so $(libdir)/dg2020.so
	chown $(OWNER).$(GROUP) $(libdir)/dg2020.so
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so
	$(LN) $(LNFLAGS) $(libdir)/dg2020_f.so $(libdir)/pulser.so
	chown $(OWNER).$(GROUP) $(libdir)/pulser.so
	-$(RM) $(RMFLAGS) $(libdir)/s_band.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_s_band.so $(libdir)/s_band.so
	chown $(OWNER).$(GROUP) $(libdir)/s_band.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_s_band.so $(libdir)/magnet.so
	chown $(OWNER).$(GROUP) $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/bh15.so $(libdir)/gaussmeter.so
	chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so
	-$(RM) $(RMFLAGS) $(libdir)/lockin.so
	$(LN) $(LNFLAGS) $(libdir)/sr510.so $(libdir)/lockin.so
	chown $(OWNER).$(GROUP) $(libdir)/lockin.so

install-crowley.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/er035m_sas.so $(libdir)/gaussmeter.so
	chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so
	-$(RM) $(RMFLAGS) $(libdir)/synthesizer.so
	$(LN) $(LNFLAGS) $(libdir)/hp8647a.so $(libdir)/synthesizer.so
	chown $(OWNER).$(GROUP) $(libdir)/synthesizer.so
	-$(RM) $(RMFLAGS) $(libdir)/boxcar.so
	$(LN) $(LNFLAGS) $(libdir)/egg4402so $(libdir)/boxcar.so
	chown $(OWNER).$(GROUP) $(libdir)/boxcar.so
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) config/hp8647a.table $(libdir)

install-moebi.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so
	$(LN) $(LNFLAGS) $(libdir)/dg2020_b.so $(libdir)/pulser.so
	chown fsc2.fsc2 $(libdir)/pulser.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_x_band.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/er035m_s.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so

install-door.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/hfs9003.so
	$(LN) $(LNFLAGS) $(libdir)/hfs9000.so $(libdir)/hfs9003.so
	chown fsc2.fsc2 $(libdir)/hfs9003.so
	-$(RM) $(RMFLAGS) $(libdir)/w_band.so
	$(LN) $(LNFLAGS) $(libdir)/keithley228a.so $(libdir)/w_band.so
	chown fsc2.fsc2 $(libdir)/w_band.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/keithley228a.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) config/hp8647a.table $(libdir)

install-isaak.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so
	$(LN) $(LNFLAGS) $(libdir)/dg2020_b.so $(libdir)/pulser.so
	chown fsc2.fsc2 $(libdir)/pulser.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_x_band.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/er035m_s.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so

install-crowley.none:
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) config/hp8647a.table $(libdir)


clean:
	-$(RM) $(RMFLAGS) *.o *_lexer.c TAGS cscope.out *~

distclean:
	$(MAKE) clean
	-$(RM) $(RMFLAGS) *.so

# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
