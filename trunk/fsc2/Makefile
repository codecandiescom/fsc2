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


# The following variables determine where the program, auxiliary files and
# the documentation will be copied to by 'make install'.

prefix           := /usr/local
bindir           := $(prefix)/bin
libdir           := $(prefix)/lib/fsc2
auxdir           := $(libdir)/aux
docdir           := $(prefix)/share/doc/fsc2
mandir           := $(prefix)/man
infodir          := $(prefix)/info


# Here the owner and the group of the program and all files that will be
# installed have to be set. I would recommended to create a special account
# for the program, with 'fsc2' being the natural choice (which is also the
# default). The group that owns the files, escpecially the executable fsc2
# should be one that allows access to the device files. See the longer
# discussion in section B of the INSTALL file.

OWNER            := fsc2
GROUP            := uucp


# The next variable sets the GPIB library to use, it must be set to either
# "LLP" for the LLP library, "NI" for the National Instruments library or
# "JTT" for the library written by me. Alternatively, if no GPIB support is
# needed define GPIB_LIBRARY as "NONE" (which is also the default if not set).
# Then the location of the GPIB configuration file is set (this is only needed
# with the National Instruments GPIB library, if not set GPIB_CONF_FILE
# defaults to /etc/gpib.conf).
# Next set the file for writing logs about the activity on the GPIB bus.
# Make sure that fsc2 or its users have write access to the directory the
# log file will be created in. If not given logs will be written to stderr.
# The next variable, GPIB_LOG_LEVEL, sets the verbosity level for the GPIB
# logs, use either HIGH, MEDIUM, LOW or OFF. If not set it defaults to LOW
# and only errors will be logged.
# Finally, if the header files for the GPIB library aren't either in
# /usr/include or /usr/include (i.e. if they are, for example, in
# /usr/include/gpib) set the last variable GPIB_HEADER_DIR to the absolute
# path of the directory where the header file(s) for the library reside.

GPIB_LIBRARY     := JTT
GPIB_CONF_FILE   := /etc/gpib.conf
GPIB_LOG_FILE    := /tmp/gpib.log
GPIB_LOG_LEVEL   := HIGH
#GPIB_HEADER_DIR := /usr/include/gpib

# NUM_SERIAL_PORTS defines the number of serial ports that are available on
# the computer and defaults to 0 if not set (in which case no serial ports
# can be used).
# The second variable, SERIAL_LOCK_DIR, is the directory where UUCP style
# lock files for the serial ports are created. This should be '/var/lock'
# according to version 2.2 of the Filesystem Hierachy Standard. Either fsc2
# itself or all its users must have read and write permissions for this
# directory. See the longer discussion in section B of the INSTALL file.

NUM_SERIAL_PORTS := 2
SERIAL_LOCK_DIR  := "/var/lock"


# This variable controls to whom mail with bug reports and crash messages will
# be send - if not set no "Bug report" button will be shown. Please change
# this only if you change the program yourself (in which case sending me bug
# reports or mails about crashes would not make too much sense).

MAIL_ADDRESS     := Jens.Toerring@physik.fu-berlin.de


##############################################################################
# Here follow settings determining the make process - which compiler and
# tools to use as well as their respective options. Better leave this alone
# if you don't have a good reason for doing changes...
# Don't use '-fomit-frame-pointer' in CFLAGS on Intel processors (unless
# you want to keep me from getting meaningful messages about crashes).
##############################################################################

OPT              := -O2

SHELL            := /bin/sh
CC               := gcc
CFLAGS           := -ggdb $(OPT) -I/usr/X11R6/include/X11 \
					-Wall \
					-Wwrite-strings \
					-Wstrict-prototypes \
					-Wmissing-declarations \
					-W \
					-Wshadow \
					-Wpointer-arith \
					-Waggregate-return \
					-Wnested-externs \
					-Wcast-align \
				    # -Wconversion
				    # -Wsign-compare      # missing in older gcc versions
				    # -Wbad-function-cast # too many false positives
				    # -Wcast-qual         # too many false positives
				    # -Wundef             # fails in bison/flex created files
				    # -ansi               # fails due to inline/asm
				    # -pedantic
				    # -DNDEBUG            # switch off build-in debugging code 
				    # -DMDEBUG            # for malloc debugging
LFLAGS			 := $(OPT) -shared -fpic  # needed for libraries
LIBS             := -L/usr/X11R6/lib -L/usr/local/lib -lforms -lX11 -lm -ldl
				    # -lmcheck   # for malloc debugging
				    # -lefence   # to run with efence
BISON            := bison
BISONFLAGS       := -d -v -t -p
FLEX             := flex
FLEXFLAGS        := -B -P
RM               := rm
RMFLAGS          := -f
LN               := ln
LNFLAGS          := -s
INSTALL          := install
EXEC_INSTALL     := install


############################################################################
###########     Don't change anything below this line      #################
###########  unless you really know what you're doing ...  #################
############################################################################

# These variables are used internally in the make process

fdir             := $(shell pwd)
sdir             := $(fdir)/src
mdir             := $(fdir)/modules
adir             := $(fdir)/aux
cdir             := $(fdir)/config


# Set up a variable with the machines FQDN

machine_name     := $(shell hostname -f)


# Check that the machine has a new libc, i.e. at least version 2, otherwise
# some problems with the older libc have to be dealt with in the program

ifneq ($(shell ls /lib/libc.so.6 2>/dev/null),/lib/libc.so.6)
	CFLAGS += -DIS_STILL_LIBC1
endif


# Here's some stuff that makes it more convenient to make and install on
# machines that are known well enough - for these machines one doesn't has
# to edit the Makefile all the time...

INSTALL_MACHINE   := install-unknown
UNINSTALL_MACHINE := uninstall-unknown

ifeq ($(machine_name),pion.anorg.chemie.uni-frankfurt.de)
	OWNER              := fsc2
	GROUP              := uucp
	GPIB_LIBRARY       := JTT
	INSTALL_MACHINE    := install-$(machine_name)
	UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),moebi.physik.fu-berlin.de)
	OWNER              := fsc2
	GROUP              := uucp
	GPIB_LIBRARY       := JTT
	INSTALL_MACHINE    := install-$(machine_name)
	UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),crowley.physik.fu-berlin.de)
	OWNER              := fsc2
	GROUP              := uucp
	GPIB_LIBRARY       := JTT
	INSTALL_MACHINE    := install-$(machine_name)
	UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),door.physik.fu-berlin.de)
	OWNER              := fsc2
	GROUP              := uucp
	GPIB_LIBRARY       := JTT
	INSTALL_MACHINE    := install-$(machine_name)
	UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),isaak.physik.fu-berlin.de)
	OWNER              := fsc2
	GROUP              := uucp
	GPIB_LIBRARY       := JTT
	INSTALL_MACHINE    := install-$(machine_name)
	UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),sunone.chemie.unibas.ch)
	OWNER              := exp
	GROUP              := exp
	GPIB_LIBRARY       := NI
	INSTALL_MACHINE    := install-$(machine_name)
	UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),tresr.physik.fu-berlin.de)
	OWNER              := fsc2
	GROUP              := uucp
	 GPIB_LIBRARY       := LLP
	 INSTALL_MACHINE    := install-$(machine_name)
	 UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

ifeq ($(machine_name),crowley.none)
	OWNER              := fsc2
	GROUP              := uucp
	 GPIB_LIBRARY       := JTT
	 INSTALL_MACHINE    := install-$(machine_name)
	 UNINSTALL_MACHINE  := uninstall-$(machine_name)
endif

# Pass lots of variables to the source files...
# For directories make sure they end with a `/'.

ifdef bindir
	CFLAGS += -Dbindir=\"$(bindir:/=)/\"
else
	CFLAGS += -Dbindir=\"$(fdir)/bin/\"
endif

ifdef libdir
	CFLAGS += -Dlibdir=\"$(libdir:/=)/\"
else
	CFLAGS += -Dlibdir=\"$(fdir)/lib/fsc2/\"
endif

ifdef auxdir
	CFLAGS += -Dauxdir=\"$(auxdir:/=)/\" -I$(adir)
else
	CFLAGS += -Dauxdir=\"$(fdir)/lib/fsc2/aux/\" -I$(adir)
endif

ifdef docdir
	CFLAGS += -Ddocdir=\"$(docdir:/=)/\"
else
	CFLAGS += -Ddocdir=\"$(fdir)/share/doc/\"
endif

ifdef GPIB_LOG_FILE
	CFLAGS += -DGPIB_LOG_FILE=\"$(GPIB_LOG_FILE)\"
else
	CFLAGS += -DGPIB_LOG_FILE=\"/tmp/gpib.log\"
endif

ifeq ($(GPIB_LOG_LEVEL),HIGH)
	CFLAGS += -DGPIB_LOG_LEVEL=LL_ALL
else
	ifeq ($(GPIB_LOG_LEVEL),MEDIUM)
		CFLAGS += -DGPIB_LOG_LEVEL=LL_CE
	else
		ifeq ($(GPIB_LOG_LEVEL),OFF)
			CFLAGS += -DGPIB_LOG_LEVEL=LL_NONE
		else
			CFLAGS += -DGPIB_LOG_LEVEL=LL_ERR
		endif
	endif
endif

ifdef GPIB_HEADER_DIR
	CFLAGS += -I$(GPIB_HEADER_DIR)
endif

ifdef MAIL_ADDRESS
	CFLAGS += -DMAIL_ADDRESS=\"$(MAIL_ADDRESS)\"
endif

ifndef OWNER
	OWNER = fsc2
endif 

ifndef GROUP
	GROUP = uucp
endif 

ifdef NUM_SERIAL_PORTS
	CFLAGS += -DNUM_SERIAL_PORTS=$(NUM_SERIAL_PORTS)
else
	CFLAGS += -DNUM_SERIAL_PORTS=0
endif

ifdef SERIAL_LOCK_DIR
	CFLAGS += -DSERIAL_LOCK_DIR=\"$(SERIAL_LOCK_DIR)\"
endif


# Define variables with full paths for the 'mail' and 'addr2line' programs
# and make sure the mail program and addr2line got found by 'which',
# otherwise don't set the variables

MAIL_PROGRAM  := $(shell which mail)
ADDR2LINE     := $(shell which addr2line)

ifneq ($(word 1,$(MAIL_PROGRAM)),which:)
	CFLAGS += -DMAIL_PROGRAM=\"$(MAIL_PROGRAM)\"
endif

ifneq ($(word 1,$(ADDR2LINE)),which:)
	CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"
endif


# Finally the definition of further variables needed

ifeq ($(GPIB_LIBRARY),LLP)
	GPIB_LIBRARY         := old
	CFLAGS               += -I/usr/local/include/gpib
	LIBS                 += -lgpib -lfl
else
	ifeq ($(GPIB_LIBRARY),NI)
		GPIB_LIBRARY     := ni
		LIBS             += -lgpib
		ifdef GPIB_CONF_FILE
			CFLAGS       += -DGPIB_CONF_FILE=\"$(GPIB_CONF_FILE)\"
		else
			CFLAGS       += -DGPIB_CONF_FILE=\"/etc/gpib.conf\"
		endif
	else
		ifeq ($(GPIB_LIBRARY),JTT)
			GPIB_LIBRARY := jtt
			LIBS         += -lgpib
		else
			GPIB_LIBRARY := none
		endif
	endif
endif


export            # export all variables to sub-makes


############## End of configuration section ##############


# Switch off all implicit rules

.SUFFIXES:
.PHONY: all install-strip install uninstall version clean distclean pack ZIP


# Default rule: make the main program, the primary lexer and the modules
# (the Makefile for modules is called via the Makefile in src)

all:
	-@grep '$$Id\: ' Makefile $(sdir)/Makefile $(sdir)/*.[chly]          \
				     $(mdir)/Makefile $(mdir)/*.[chl]                    \
					 $(cdir)/Functions $(cdir)/Devices $(cdir)/*.conf |  \
		grep -v 'sed\|grep' |                                            \
			sed -e 's/^.*$$Id\: \(.*\) jens Exp $$.*/\1/g' |             \
				gzip -c -9 | uuencode - > version.ugz
	$(MAKE) -C src all
	$(MAKE) -C utils all
	$(MAKE) -C doc all


# Install the program and all the other stuff.

install-strip:
	$(MAKE) EXEC_INSTALL='$(INSTALL) -s' install

install:
	$(INSTALL) -d $(libdir)
	$(INSTALL) -d $(auxdir)

	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(cdir)/Functions $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(cdir)/Devices $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/*.xpm $(auxdir)

	$(MAKE) -C src install
	$(MAKE) -C modules install
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) version.ugz $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(cdir)/hp8647a.table $(libdir)
	$(MAKE) -C utils install
	$(MAKE) -C scripts install
	$(MAKE) -C doc install



# How to get rid of everything that got installed - requires that the Makefile
# hasn't been changed significantly since the installation!

uninstall:
	-$(RM) $(RMFLAGS) $(libdir)/hp8647a.table
	-$(RM) $(RMFLAGS) $(libdir)/version.ugz
	-$(RM) $(RMFLAGS) $(libdir)/Functions
	-$(RM) $(RMFLAGS) $(libdir)/Devices
	-for file in $(adir)/*.xpm; do                     \
		$(RM) $(RMFLAGS) $(auxdir)/`basename $$file`;  \
	done
	$(MAKE) -C src uninstall
	$(MAKE) -C modules uninstall
	$(MAKE) -C utils uninstall
	$(MAKE) -C scripts uninstall
	$(MAKE) -C doc uninstall
	-if [ -z "`ls -A $(bindir)`" ]; then  \
		rmdir $(bindir);                  \
	fi
	-if [ -z "`ls -A $(auxdir)`" ]; then  \
		rmdir $(auxdir);                  \
	fi
	-if [ -z "`ls -A $(libdir)`" ]; then  \
		rmdir $(libdir);                  \
	fi


# Unpacks the (local) version file and writes its content to stdout

version:
	-@if [ -e version.ugz ]; then             \
		uudecode version.ugz | gunzip -c -q;  \
	fi


# `clean' deletes all the files remaining from the compilation and not needed
# for an installation while `distclean' also removes all executables, shared
# libraries and documentation

clean:
	$(MAKE) -C src clean
	$(MAKE) -C modules clean
	$(MAKE) -C doc clean

distclean:
	$(MAKE) -C src distclean
	$(MAKE) -C modules distclean
	$(MAKE) -C doc distclean
	$(MAKE) -C utils distclean
	-$(RM) $(RMFLAGS) version.ugz makelog *~


# Clean up everything for a distribution and create a zipped tarball

pack:
	$(MAKE) distclean
	cd ..; tar -c fsc2 | gzip -c -9 > fsc2.tgz


# Create zipped tarball and write it to the ZIP drive (at home or on isaak
# also mount and unmount the ZIP drive, otherwise assume that it's already
# mounted at isaak)

ZIP:
	$(MAKE) pack
	if [    "$(machine_name)" = "crowley.none"                       \
		 -o "$(machine_name)" = "isaak.physik.fu-berlin.de" ]; then  \
		if ! `grep ZIP /etc/mtab > /dev/null`; then                  \
			sleep 1;                                                 \
			mount /ZIP;                                              \
		fi;                                                          \
		cp ../fsc2.tgz /ZIP;                                         \
		umount /ZIP;                                                 \
	else                                                             \
		scp ../fsc2.tgz isaak.physik.fu-berlin.de:/ZIP;              \
	fi
