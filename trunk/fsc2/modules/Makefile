# $Id$
#
# Copyright (C) 1999-2003 Jens Thoms Toerring
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
hp864_list    := hp8647a hp8648b

simp_modules  := User_Functions.c sr510.c sr530.c sr810.c sr830.c          \
				 aeg_s_band.c aeg_x_band.c er035m.c er035m_s.c hp5340a.c   \
				 er035m_sa.c er035m_sas.c bh15.c keithley228a.c egg4402.c  \
				 kontron4060.c lakeshore330.c pt2025.c er032m.c ips20_4.c  \
				 $(addsuffix .c,$(s_band_list)) ni6601.c me6000.c

comp_modules  := dg2020_f dg2020_b hfs9000 ep385 rs690 er023m lecroy9400  \
				 hp8672a $(hp864_list) $(tds_list) rs_spec10

dg2020_f      := dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c  \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b      := dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c  \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
hfs9000       := hfs9000.c hfs9000_gen.c hfs9000_pulse.c hfs9000_util.c  \
				 hfs9000_init.c hfs9000_run.c hfs9000_gpib.c
ep385         := ep385.c ep385_gen.c ep385_pulse.c ep385_util.c  \
				 ep385_init.c ep385_run.c ep385_gpib.c
rs690         := rs690.c rs690_gen.c rs690_pulse.c rs690_util.c  \
				 rs690_init.c rs690_run.c rs690_gpib.c
tds754a       := tds754a.c tds754a_gpib.c tds754a_util.c
tds744a       := tds744a.c tds744a_gpib.c tds744a_util.c
tds540        := tds540.c  tds540_gpib.c  tds540_util.c
tds520c       := tds520c.c tds520c_gpib.c tds520c_util.c
tds520a       := tds520a.c tds520a_gpib.c tds520a_util.c
tds520        := tds520.c  tds520_gpib.c  tds520_util.c
hp8647a       := hp8647a.c hp8647a_util.c hp8647a_gpib.c hp8647a_lexer.c
hp8648b       := hp8648b.c hp8648b_util.c hp8648b_gpib.c hp8648b_lexer.c
hp8672a       := hp8672a.c hp8672a_util.c hp8672a_gpib.c hp8672a_lexer.c
er023m        := er023m.c  er023m_addon.c er023m_gpib.c
lecroy9400    := lecroy9400.c lecroy9400_gpib.c  lecroy9400_util.c
rs_spec10     := rs_spec10.c rs_spec10_int.c rs_spec10_util.c


# For modules that require header files in non-standard places where the
# compiler would find them automatically (i.e. /usr/include or
# /usr/local/include) you must specify the full search path(s) here.

rs_spec10_incl := /usr/local/include/pvcam
#rs_spec10_incl := /usr/local/pvcam/examples


# For modules that need to be linked against special libraries a variable
# needs to be defined here, consisting of the name of the module followed
# by the string "_libs". It got to be assigned the name(s) of the required
# libraries. If a library is not in one of the standard places where the
# linker looks for libraries (e.g. /usr/lib, /usr/local/lib) it must be
# specified with its absolute path.

me6000_libs    := libme6x00.so
ni6601_libs    := libni6601.so
rs_spec10_libs := libpvcam.so


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
		install-single.physik.fu-berlin.de              \
		install-tresr.physik.fu-berlin.de               \
		install-sunone.chemie.unibas.ch                 \
		install-crowley.none                            \
		install-dorimare.nowhere                        \
		uninstall-pion.anorg.chemie.uni-frankfurt.de    \
		uninstall-crowley.physik.fu-berlin.de           \
		uninstall-door.physik.fu-berlin.de              \
		uninstall-isaak.physik.fu-berlin.de             \
		uninstall-moebpc3.physik.fu-berlin.de           \
		uninstall-single.physik.fu-berlin.de            \
		uninstall-tresr.physik.fu-berlin.de             \
		uninstall-sunone.chemie.unibas.ch               \
		uninstall-crowley.none                          \
		uninstall-dorimare.nowhere


# Making the modules consisting of just one file is simple, to make the
# more complicated modules we need to create temporary makefiles from a
# template and then invoke them. For the Tektronix digitizers and the
# HP864* synthesizers also the sources are created from templates.

all:
	$(MAKE) $(simp_modules:.c=.so)
	@for n in $(tds_list); do                                      \
		o=`echo $$n | tr "a-z" "A-Z"`;                             \
		for m in "" _util _gpib; do                                \
			if [ ! -e $$n$$m.c -o tds$$m.c.tmpl -nt $$n$$m.c       \
				 -o $$n.h -nt $$n$$m.c ]; then                     \
				sed "s/tds_tmpl/$$n/g;s/TDS_TMPL/$$o/g"            \
					tds$$m.c.tmpl > $$n$$m.c;                      \
			fi;                                                    \
		done;                                                      \
	done;
	@for n in $(hp864_list); do                                    \
		o=`echo $$n | tr "a-z" "A-Z"`;                             \
		for m in .c _util.c _gpib.c _lexer.l; do                   \
			if [ ! -e $$n$$m -o hp864$$m.tmpl -nt $$n$$m           \
				 -o $$n.h -nt $$n$$m ]; then                       \
				sed "s/hp864_tmpl/$$n/g;s/HP864_TMPL/$$o/g"        \
					hp864$$m.tmpl > $$n$$m;                        \
			fi;                                                    \
		done;                                                      \
	done;
	@for m in $(shell echo $(comp_modules) | cut -f1- -d' '); do   \
		sed -e "s/##/$$m/g" Makefile.tmpl > $$m.make &&            \
		$(MAKE) -f $$m.make;                                       \
		$(RM) $(RMFLAGS) $$m.make;                                 \
	done;


# The following rule looks a bit complicated because we have to check if
# additional libraries are required by each module. These libraries have
# to defined by a variables consisting of the name of the module and the
# the string "_libs". If no additional libraries are requires we compile
# the module immediately. Otherwise we've got to distinguish between
# libraries given with an absolute path (which are passed to gcc just as
# they are) and libraries without a path, that are supposed to be in the
# directories searched by the linker. For these we must remove the leading
# "lib" and the trailing".so" extension and prepend the "-l" option. Before
# we actually use the library we check if they really exist by an extra
# call of gcc - if it fails the module can't be created.

$(simp_modules:.c=.so): %.so: %.o	
	@if [ -z "$($(patsubst %.so,%_libs,$@))" ]; then               \
		echo "$(CC) $(LFLAGS) $^ -o $@";                           \
		$(CC) $(LFLAGS) $^ -o $@;                                  \
		chmod 644 $@;                                              \
	else                                                           \
		for l in "$($(patsubst %.so,%_libs,$@))"; do               \
			if [ "$(notdir $$l)" == "$$l" ]; then                  \
				l=$${l%.so};                                       \
				libs="$$libs -l$${l#lib}";                         \
			else                                                   \
				libs="$$libs $$l";                                 \
			fi;                                                    \
		done;                                                      \
		echo "$(CC) $(LFLAGS) $^ $$libs -o $@";                    \
		$(CC) $$libs -shared -o $@.lib_test 2>/dev/null;           \
		if [ -e $@.lib_test ]; then                                \
			$(RM) $(RMFLAGS) $@.lib_test;                          \
			$(CC) $(LFLAGS) $^ $$libs -o $@;                       \
			chmod 644 $@;                                          \
		else                                                       \
			echo "warning: $@ not created, libraries not found.";  \
		fi;                                                        \
	fi


# Since some modules may require header files for additional libraries.
# We prepend each value in variables with a name starting with the module
# name and followed by the string "_incl" with "-I" and pass this to the
# compiler.

$(simp_modules:.c=.o): %.o: %.c $(cdir)/%.conf $(src_headers)
	@incl="$(foreach i,$($(patsubst %.o,%_incl,$@)),-I$(i))";      \
	echo "$(CC) $$incl $(CFLAGS) -c $(LFLAGS) $< -o $@";           \
	$(CC) $$incl $(CFLAGS) -c $(LFLAGS) $< -o $@


# Also the modules for controlling the Frankfurt S-band magnet are made
# from a template file

$(addsuffix .c,$(s_band_list)): %.c: s_band_magnet.c.tmpl
	n=$(subst .c,,$(subst s_band_magnet_,,$@));                    \
	o=`echo $$n | tr "a-z" "A-Z"`;                                 \
	sed "s/s_band_magnet_tmpl/s_band_magnet_$$n/g;s/S_BAND_MAGNET_TMPL/S_BAND_MAGNET_$$o/g" $^ > $@


# How to install the modules

install:
	$(INSTALL) -d $(libdir)
	$(EXEC_INSTALL) -m  644 -o $(OWNER) -g $(GROUP) *.so $(libdir)
	if [ "$(INSTALL_MACHINE)" != "install-unknown" ]; then         \
		$(MAKE) $(INSTALL_MACHINE);                                \
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

install-single.physik.fu-berlin.de:
	$(MAKE) $(UNINSTALL_MACHINE)
	-$(LN) $(LNFLAGS) $(libdir)/ni6601.so $(libdir)/counter.so
	-chown $(OWNER).$(GROUP) $(libdir)/counter.so
	-$(LN) $(LNFLAGS) $(libdir)/me6000.so $(libdir)/dac.so
	-chown $(OWNER).$(GROUP) $(libdir)/dac.so

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

uninstall-single.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/counter.so
	-$(RM) $(RMFLAGS) $(libdir)/dac.so

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
	-for n in $(tds_list); do                    \
		for m in "" _util _gpib; do              \
			$(RM) $(RMFLAGS) $$n$$m.c;           \
		done;                                    \
	done;
	-for n in $(hp864_list); do                  \
		for m in .c _util.c _gpib.c _lexer.l; do \
			$(RM) $(RMFLAGS) $$n$$m;             \
		done;                                    \
	done;
	-for n in $(s_band_list); do                 \
		$(RM) $(RMFLAGS) $$n.c;                  \
	done;
	-$(RM) $(RMFLAGS) *.so

# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
