# $Id$
#
# Copyright (C) 1999-2002 Jens Thoms Toerring
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


s_band_list   := s_band_magnet_narrow s_band_magnet_broad
tds_list      := tds754a tds744a tds540 tds520c tds520a tds520

simp_modules  := User_Functions.c sr510.c sr530.c sr810.c sr830.c \
				 aeg_s_band.c aeg_x_band.c er035m.c er035m_s.c hp5340a.c \
				 er035m_sa.c er035m_sas.c bh15.c keithley228a.c egg4402.c \
				 kontron4060.c lakeshore330.c pt2025.c er032m.c ips20_4.c \
				 $(addsuffix .c,$(s_band_list))

comp_modules  := dg2020_f dg2020_b hfs9000 ep385 rs690 er023m lecroy9400 \
				 hp8647a hp8672a $(tds_list)

dg2020_f      := dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b      := dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
hfs9000       := hfs9000.c hfs9000_gen.c hfs9000_pulse.c hfs9000_util.c \
				 hfs9000_init.c hfs9000_run.c hfs9000_gpib.c
ep385         := ep385.c ep385_gen.c ep385_pulse.c ep385_util.c \
				 ep385_init.c ep385_run.c ep385_gpib.c
rs690         := rs690.c rs690_gen.c rs690_pulse.c rs690_util.c \
				 rs690_init.c rs690_run.c rs690_gpib.c
tds754a       := tds754a.c tds754a_gpib.c tds754a_util.c
tds744a       := tds744a.c tds744a_gpib.c tds744a_util.c
tds540        := tds540.c  tds540_gpib.c  tds540_util.c
tds520c       := tds520c.c tds520c_gpib.c tds520c_util.c
tds520a       := tds520a.c tds520a_gpib.c tds520a_util.c
tds520        := tds520.c  tds520_gpib.c  tds520_util.c
hp8647a       := hp8647a.c hp8647a_util.c hp8647a_gpib.c hp8647a_lexer.c
hp8672a       := hp8672a.c hp8672a_util.c hp8672a_gpib.c hp8672a_lexer.c
er023m        := er023m.c  er023m_addon.c er023m_gpib.c
lecroy9400    := lecroy9400.c  lecroy9400_gpib.c  lecroy9400_util.c

src_headers   := $(addprefix $(sdir)/,$(headers)) $(sdir)/fsc2_module.h

CFLAGS        += -I$(sdir) -I$(cdir)


export        # export all symbols to the sub-makefiles


.SUFFIXES:
.PHONY: all install clean distclean                     \
		install-pion.anorg.chemie.uni-frankfurt.de      \
		install-crowley.physik.fu-berlin.de             \
		install-door.physik.fu-berlin.de                \
		install-isaak.physik.fu-berlin.de               \
		install-moebpc3.physik.fu-berlin.de             \
		install-tresr.physik.fu-berlin.de               \
		install-sunone.chemie.unibas.ch                 \
		install-crowley.none                            \
		install-dorimare.nowhere                        \
		uninstall-pion.anorg.chemie.uni-frankfurt.de    \
		uninstall-crowley.physik.fu-berlin.de           \
		uninstall-door.physik.fu-berlin.de              \
		uninstall-isaak.physik.fu-berlin.de             \
		uninstall-moebpc3.physik.fu-berlin.de           \
		uninstall-tresr.physik.fu-berlin.de             \
		uninstall-sunone.chemie.unibas.ch               \
		uninstall-crowley.none                          \
		uninstall-dorimare.nowhere


# Making the modules consisting of just one file is simple, to make the
# more complicated modules we need to create temporary makefiles from a
# template and then invoke them. For the Tektronix digitizers even the
# sources are created from templates.

all: $(simp_modules:.c=.so)
	for n in $(tds_list); do \
		o=`echo $$n | tr "a-z" "A-Z"`; \
		for m in "" _util _gpib; do \
			if [ ! -e $$n$$m.c -o tds$$m.c.tmpl -nt $$n$$m.c \
				 -o $$n.h -nt $$n$$m.c ]; then \
				sed "s/tds_tmpl/$$n/g;s/TDS_TMPL/$$o/g" \
					tds$$m.c.tmpl > $$n$$m.c; \
			fi; \
		done; \
	done;
	for m in $(shell echo $(comp_modules) | cut -f1- -d' '); do \
		sed -e "s/##/$$m/g" Makefile.tmpl > $$m.make; \
		$(MAKE) -f $$m.make; \
		$(RM) $(RMFLAGS) $$m.make; \
	done;

$(simp_modules:.c=.so): %.so: %.o
	$(CC) $(LFLAGS) $^ -o $@
	chmod 644 $@

$(simp_modules:.c=.o): %.o: %.c $(cdir)/%.conf $(src_headers)
	$(CC) $(CFLAGS) -c $(LFLAGS) $< -o $@

# Also the modules for controlling the Frankfurt S-band magnet are made
# from a template file

$(addsuffix .c,$(s_band_list)): %.c: s_band_magnet.c.tmpl
	n=$(subst .c,,$(subst s_band_magnet_,,$@)); \
	o=`echo $$n | tr "a-z" "A-Z"`; \
	sed "s/s_band_magnet_tmpl/s_band_magnet_$$n/g;s/S_BAND_MAGNET_TMPL/S_BAND_MAGNET_$$o/g" $^ > $@


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
# the power supply in EDL programs. Already existing links must be deleted
# before creating new ones!

install-pion.anorg.chemie.uni-frankfurt.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/dg2020_f.so $(libdir)/dg2020.so
	-chown $(OWNER).$(GROUP) $(libdir)/dg2020.so
	-$(LN) $(LNFLAGS) $(libdir)/dg2020_f.so $(libdir)/pulser.so
	-chown $(OWNER).$(GROUP) $(libdir)/pulser.so
	-$(LN) $(LNFLAGS) $(libdir)/aeg_s_band.so $(libdir)/s_band.so
	-chown $(OWNER).$(GROUP) $(libdir)/s_band.so
	-$(LN) $(LNFLAGS) $(libdir)/s_band_magnet_broad.so $(libdir)/magnet_b.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet_b.so
	-$(LN) $(LNFLAGS) $(libdir)/s_band_magnet_narrow.so $(libdir)/magnet_n.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet_n.so
	-$(LN) $(LNFLAGS) $(libdir)/bh15.so $(libdir)/gaussmeter.so
	-chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so
	-$(LN) $(LNFLAGS) $(libdir)/sr530.so $(libdir)/lockin.so
	-chown $(OWNER).$(GROUP) $(libdir)/lockin.so

install-crowley.physik.fu-berlin.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/er035m_sas.so $(libdir)/gaussmeter.so
	-chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so
	-$(LN) $(LNFLAGS) $(libdir)/hp8647a.so $(libdir)/synthesizer.so
	-chown $(OWNER).$(GROUP) $(libdir)/synthesizer.so
	-$(LN) $(LNFLAGS) $(libdir)/egg4402so $(libdir)/boxcar.so
	-chown $(OWNER).$(GROUP) $(libdir)/boxcar.so

install-door.physik.fu-berlin.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/hfs9000.so $(libdir)/hfs9003.so
	-chown $(OWNER).$(GROUP) $(libdir)/hfs9003.so
	-$(LN) $(LNFLAGS) $(libdir)/keithley228a.so $(libdir)/w_band.so
	-chown $(OWNER).$(GROUP) $(libdir)/w_band.so
	-$(LN) $(LNFLAGS) $(libdir)/keithley228a.so $(libdir)/magnet.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet.so

install-isaak.physik.fu-berlin.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/dg2020_b.so $(libdir)/pulser.so
	-chown $(OWNER).$(GROUP) $(libdir)/pulser.so
	-$(LN) $(LNFLAGS) $(libdir)/aeg_x_band.so $(libdir)/magnet.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet.so
	-$(LN) $(LNFLAGS) $(libdir)/er035m_s.so $(libdir)/gaussmeter.so
	-chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so

install-moebpc3.physik.fu-berlin.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/ips20_4.so $(libdir)/magnet.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet.so
	-$(LN) $(LNFLAGS) $(libdir)/sr830.so $(libdir)/lockin.so
	-chown $(OWNER).$(GROUP) $(libdir)/lockin.so

install-tresr.physik.fu-berlin.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/er032m.so $(libdir)/magnet.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet.so
	-$(LN) $(LNFLAGS) $(libdir)/er023m.so $(libdir)/lockin.so
	-chown $(OWNER).$(GROUP) $(libdir)/lockin.so

install-sunone.chemie.unibas.ch:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/er032m.so $(libdir)/magnet.so
	-chown $(OWNER).$(GROUP) $(libdir)/magnet.so

install-crowley.none:
	$(MAKE) $(UNINSTALL_MACHINE)

install-dorimare.nowhere:
	$(MAKE) $(UNINSTALL_MACHINE)


# How to uninstall the modules

uninstall:
	-for f in *.so; do                     \
		$(RM) $(RMFLAGS) $(libdir)/$$f;    \
	done
	if [ "$(UNINSTALL_MACHINE)" != "uninstall-unknown" ]; then  \
		$(MAKE) $(UNINSTALL_MACHINE);                           \
	fi

uninstall-pion.anorg.chemie.uni-frankfurt.de:
	-$(RM) $(RMFLAGS) $(libdir)/dg2020.so
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so
	-$(RM) $(RMFLAGS) $(libdir)/s_band.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so
	-$(RM) $(RMFLAGS) $(libdir)/lockin.so

uninstall-crowley.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so
	-$(RM) $(RMFLAGS) $(libdir)/synthesizer.so
	-$(RM) $(RMFLAGS) $(libdir)/boxcar.so

uninstall-door.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/hfs9003.so
	-$(RM) $(RMFLAGS) $(libdir)/w_band.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so

uninstall-isaak.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so

uninstall-moebpc3.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/lockin.so

uninstall-tresr.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so
	-$(RM) $(RMFLAGS) $(libdir)/lockin.so

uninstall-sunone.chemie.unibas.ch:
	-$(RM) $(RMFLAGS) $(libdir)/magnet.so

uninstall-crowley.none:

uninstall-dorimare.nowhere::


# How to clean up the mess we created

clean:
	-$(RM) $(RMFLAGS) *.o *_lexer.c cscope.out *.make *~

distclean:
	$(MAKE) clean
	-for n in $(tds_list); do \
		for m in "" _util _gpib; do \
			$(RM) $(RMFLAGS) $$n$$m.c; \
		done; \
	done;
	-for n in $(s_band_list); do \
		$(RM) $(RMFLAGS) $$n.c; \
	done;
	-$(RM) $(RMFLAGS) *.so

# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
