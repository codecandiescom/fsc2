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


# Lists of modules that get created from the same template files

s_band_list     := s_band_magnet_narrow s_band_magnet_broad
tds_list        := tds754a tds744a tds540 tds520c tds520a tds520
hp864_list      := hp8647a hp8648b


# List of modules that are to be made from a single source file only

simp_modules    := User_Functions sr510 sr530 sr810 sr830 aeg_s_band        \
				   aeg_x_band er035m er035m_s hp5340a er035m_sa er035m_sas  \
				   bh15 keithley228a egg4402 kontron4060 lakeshore330       \
				   pt2025 er032m ips20_4 ips120_10 $(s_band_list)           \
				   thurlby330 hjs_daadc gg_chopper


# For each of the "complicated" modules a list of all source files they are
# to be created from is needed

dg2020_f        := dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
			       dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b        := dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
			       dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
hfs9000         := hfs9000.c hfs9000_gen.c hfs9000_pulse.c hfs9000_util.c  \
			       hfs9000_init.c hfs9000_run.c hfs9000_gpib.c
ep385           := ep385.c ep385_gen.c ep385_pulse.c ep385_util.c  \
			       ep385_init.c ep385_run.c ep385_gpib.c
rs690           := rs690.c rs690_gen.c rs690_pulse.c rs690_util.c  \
			       rs690_init.c rs690_run.c rs690_gpib.c
tds754a         := tds754a.c tds754a_gpib.c tds754a_util.c
tds744a         := tds744a.c tds744a_gpib.c tds744a_util.c
tds540          := tds540.c  tds540_gpib.c  tds540_util.c
tds520c         := tds520c.c tds520c_gpib.c tds520c_util.c
tds520a         := tds520a.c tds520a_gpib.c tds520a_util.c
tds520          := tds520.c  tds520_gpib.c  tds520_util.c
hp8647a         := hp8647a.c hp8647a_util.c hp8647a_gpib.c hp8647a_lexer.c
hp8648b         := hp8648b.c hp8648b_util.c hp8648b_gpib.c hp8648b_lexer.c
hp8672a         := hp8672a.c hp8672a_util.c hp8672a_gpib.c hp8672a_lexer.c
er023m          := er023m.c  er023m_addon.c er023m_gpib.c
lecroy9400      := lecroy9400.c lecroy9400_gpib.c  lecroy9400_util.c
lecroy9400_s    := lecroy9400_s.c lecroy9400_serial.c  lecroy9400_util.c
rs_spec10       := rs_spec10.c rs_spec10_int.c rs_spec10_util.c
spectrapro_300i := spectrapro_300i.c spectrapro_300i_ll.c  \
				   spectrapro_300i_lexer.c
hjs_attenuator  := hjs_attenuator.c hjs_attenuator_lexer.c
hjs_sfc         := hjs_sfc.c hjs_sfc_lexer.c
hjs_fc          := hjs_fc.c hjs_fc_lexer.c
spex_cd2a       := spex_cd2a.c spex_cd2a_ll.c
pci_mio_16e_1   := pci_mio_16e_1.c pci_mio_16e_1_ai.c pci_mio_16e_1_ao.c \
				   pci_mio_16e_1_dio.c pci_mio_16e_1_gpct.c \
				   pci_mio_16e_1_msc.c pci_mio_16e_1_util.c
rs_sml01        := rs_sml01.c rs_sml01_util.c rs_sml01_gpib.c rs_sml01_lexer.c
rb_pulser       := rb_pulser.c rb_pulser_gen.c rb_pulser_pulse.c \
				   rb_pulser_util.c rb_pulser_init.c rb_pulser_run.c \
				   rb_pulser_ll.c


# List of all "complicated" modules that are to be made from more than a
# single source file (plus a header file)

comp_modules    := dg2020_f dg2020_b hfs9000 ep385 rs690 er023m lecroy9400  \
				   hp8672a $(hp864_list) $(tds_list) spectrapro_300i        \
				   hjs_attenuator hjs_sfc lecroy9400_s spex_cd2a rs_sml01


# Additional modules that only get created on demand (they require some
# additional libraries that not installed on all systems)

ifdef WITH_ME6000
simp_modules    += me6000
endif

ifdef WITH_NI6601
simp_modules    += ni6601
endif

ifdef WITH_WITIO_48
simp_modules    += witio_48 bnm12
comp_modules    += hjs_fc
endif

ifdef WITH_SPEC10
comp_modules    += rs_spec10
endif

ifdef WITH_PCI_MIO_16E_1
comp_modules    += pci_mio_16e_1
endif

ifdef WITH_RULBUS
simp_modules    += rb8509
comp_modules    += rb_pulser
endif

src_headers     := $(addprefix $(sdir)/,$(headers)) $(sdir)/fsc2_module.h

INCLUDES        += -I$(sdir) -I$(cdir)


export        # export all symbols to the sub-makefiles


.SUFFIXES:

.PHONY: all modules install uninstall clean distclean   \


# Making the modules consisting of just one file is simple, to make the
# more complicated modules we need to create temporary makefiles from a
# template and then invoke them. For the Tektronix digitizers and the
# HP864* synthesizers also the sources as well as the linker map files
# are created from templates.

all:
	$(MAKE) -C .. modules

modules:
	$(MAKE) $(addsuffix .so,$(simp_modules))
	@for n in $(tds_list); do                                       \
		o=`echo $$n | tr "a-z" "A-Z"`;                              \
		for m in "" _util _gpib; do                                 \
			if [ ! -e $$n$$m.c -o tds$$m.c.tmpl -nt $$n$$m.c        \
				 -o $$n.h -nt $$n$$m.c ]; then                      \
				sed "s/tds_tmpl/$$n/g;s/TDS_TMPL/$$o/g"             \
					tds$$m.c.tmpl > $$n$$m.c;                       \
			fi;                                                     \
		done;                                                       \
		if [ ! -e tds$$n.map -a -e tds.map.tmpl ]; then             \
				sed "s/tds_tmpl/$$n/g;s/TDS_TMPL/$$o/g"             \
					tds.map.tmpl > $$n.map;                         \
		fi;                                                         \
	done;
	@for n in $(hp864_list); do                                     \
		o=`echo $$n | tr "a-z" "A-Z"`;                              \
		for m in .c _util.c _gpib.c _lexer.l; do                    \
			if [ ! -e $$n$$m -o hp864$$m.tmpl -nt $$n$$m            \
				 -o $$n.h -nt $$n$$m ]; then                        \
				sed "s/hp864_tmpl/$$n/g;s/HP864_TMPL/$$o/g"         \
					hp864$$m.tmpl > $$n$$m;                         \
			fi;                                                     \
		done;                                                       \
		if [ ! -e $$n.map -a -e hp864.map.tmpl ]; then              \
				sed "s/hp864_tmpl/$$n/g;s/HP864_TMPL/$$o/g"         \
					hp864.map.tmpl > $$n.map;                       \
		fi;                                                         \
	done;
	@for m in $(shell echo $(comp_modules) | cut -f1- -d' '); do    \
		sed "s/##/$$m/g" Makefile.tmpl > $$m.make &&                \
		$(MAKE) -f $$m.make;                                        \
		$(RM) $(RMFLAGS) $$m.make;                                  \
	done;


# The following rule looks a bit complicated because we have to check if
# additional libraries are required for the individual modules. These
# libraries have to be defined by a variables consisting of the name of
# the module and the the string "_libs". If no additional libraries are
# requires we compile the module immediately. Otherwise we've got to
# distinguish between libraries given with an absolute path (which are
# passed to gcc just as they are) and libraries without a path, which are
# supposed to be in the directories searched by the linker. For these we
# must remove the leading "lib" and the trailing".so" extension and
# prepend the "-l" option. Before we actually use the library we check if
# they really exist by an extra call of gcc - if it fails the module can't
# be created (which takes care of people who obviously don't need the
# module because they didn't care to install the required libraries).

$(addsuffix .so,$(simp_modules)): %.so: %.o	
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


# Compile each simple module to an object file (take care to pass the
# include directories for possibly required additional header files to
# the compiler, they must be set by a variable starting with the name
# of the module and followed by "_incl")

$(addsuffix .o,$(simp_modules)): %.o: %.c $(cdir)/%.conf $(src_headers)
	@incl="$(foreach i,$($(patsubst %.o,%_incl,$@)),-I$(i))";         \
	echo "$(CC) $$incl $(INCLUDES) $(CFLAGS) -c $(LFLAGS) $< -o $@";  \
	$(CC) $$incl $(INCLUDES) $(CFLAGS) -c $(LFLAGS) $< -o $@


# Also the modules for controlling the Frankfurt S-band magnet are made
# from a template file

$(addsuffix .c,$(s_band_list)): %.c: s_band_magnet.c.tmpl
	n=$(subst .c,,$(subst s_band_magnet_,,$@));                    \
	o=`echo $$n | tr "a-z" "A-Z"`;                                 \
	sed "s/s_band_magnet_tmpl/s_band_magnet_$$n/g;s/S_BAND_MAGNET_TMPL/S_BAND_MAGNET_$$o/g" $^ > $@


# How to install the modules

-include $(mchdir)/$(machine_name)-install

install:
	$(INSTALL) -d $(libdir)
	$(EXEC_INSTALL) -m  644 -o $(OWNER) -g $(GROUP) *.so $(libdir)
	if [ -r $(mchdir)/$(machine_name)-install ]; then  \
		$(MAKE) install-$(machine_name);               \
	fi

# How to uninstall the modules

uninstall:
	-for f in *.so; do                                 \
		$(RM) $(RMFLAGS) $(libdir)/$$f;                \
	done
	if [ -r $(mchdir)/$(machine_name)-install ]; then  \
		$(MAKE) uninstall-$(machine_name);             \
	fi


# How to clean up the mess we created

clean:
	-$(RM) $(RMFLAGS) *.o *_lexer.c cscope.out *.make *~

distclean:
	$(MAKE) clean
	-for n in $(tds_list); do                     \
		for m in "" _util _gpib; do               \
			$(RM) $(RMFLAGS) $$n$$m.c;            \
		done;                                     \
		$(RM) $(RMFLAGS) $$n.map;                 \
	done;
	-for n in $(hp864_list); do                   \
		for m in .c _util.c _gpib.c _lexer.l; do  \
			$(RM) $(RMFLAGS) $$n$$m;              \
		done;                                     \
		$(RM) $(RMFLAGS) $$n.map;                 \
	done;
	-for n in $(s_band_list); do                  \
		$(RM) $(RMFLAGS) $$n.c;                   \
	done;
	-$(RM) $(RMFLAGS) *.so

# don't automatically delete intermediate files created by flex

.PRECIOUS: %.c
