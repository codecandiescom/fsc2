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

prefix         = /usr/local
bindir         = $(prefix)/bin
libdir         = $(prefix)/lib/fsc2
auxdir         = $(libdir)/aux
docdir         = $(prefix)/share/doc/fsc2
mandir         = $(prefix)/man
infodir        = $(prefix)/info


# The first variable sets the GPIB library to use, it must be set to either
# LLP, NI or JTT for the LLP library, the National Instruments library or
# the library written by me. Alternatively, if no GPIB support is needed
# define GPIB_LIBRARY as NONE (which is also the default if not set).
# Then the location of the GPIB configuration file is set (this is only needed
# with the National Instruments GPIB library, if not set GPIB_CONF_FILE
# defaults to /etc/gpib.conf).
# Next set the file for writing logs about the activity on the GPIB bus.
# If not given logs will be written to stderr.
# Finally, set the verbosity level of the GPIB logs, use HIGH, MEDIUM, LOW
# or OFF. If not set it defaults to LOW, i.e. only errors will be logged.

GPIB_LIBRARY     = JTT
GPIB_CONF_FILE   = /etc/gpib.conf
GPIB_LOG_FILE    = /tmp/gpib.log
GPIB_LOG_LEVEL   = HIGH


# Here the owner of the program of all files that will be installed as well
# as the group the owner belongs to has to be set. It is highly recommended
# to create a special account and group for the program, with 'fsc2' being
# the natural choice (which is also the default). For a local install, i.e.
# if the program will be used by one person only and everything is installed
# in his/her home directory this persons account and group can be used.
#
# A further important point is that due to the necessity to access physical
# devices the program needs the permissions to access these devices, i.e.
# the corresponding device files. Therefor the permissions for these device
# files must be set so that either everyone is allowed to access these files
# or (which is probably the saver alternative) that fsc2 itself is allowed
# to access the devices, i.e. by including just fsc2 (and not everyone) into
# the group that has the necessary access permissions.

OWNER            = fsc2
GROUP            = fsc2


# This variable defines the number of serial ports that are available on
# the computer.

NUM_SERIAL_PORTS = 2


# Only leave the next line in if your machine does have a i386 type processor,
# i.e. a i386, i486, Pentium (Pro, II, III or 4) or something compatible like
# a K6, K7, Celeron, Duron, Athlon etc., otherwise comment it out or delete it.
# (Some stuff that helps me with debugging the program uses a bit of assembler,
# so it only works on processors with an Intel type architecture. Removing the
# following line doesn't influence the basic functionality, it just makes it a
# little bit harder to trace bugs...)

IS_I386          = 1


##############################################################################
# Here follow settings determining the make process - which compiler and
# tools to use as well as their respective options. Better leave this alone
# if you don't have a good reason...
##############################################################################

SHELL          = /bin/sh
CC             = gcc
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
				 -Wmissing-declarations -W
				 # -DMDEBUG # for malloc debugging
				 # -DDEBUG
				 # -DMAX_DEBUG # for debugging of digitizer
LD             = ld
LIBS           = -L/usr/X11R6/lib -L/usr/local/lib -lforms -lX11 \
				 -lfl -lm -ldl
				 # -lmcheck   # for malloc debugging
				 # -lefence   # to run with efence
BISON          = bison
BISONFLAGS     = -d -v -t -p
FLEX           = flex
FLEXFLAGS      = -B -P
RM             = rm
RMFLAGS        = -f
LN             = ln
LNFLAGS        = -s
INSTALL        = install
EXEC_INSTALL   = install


############################################################################
###########     Don't change anything below this line      #################
########### (unless you really know what you're doing ...) #################
############################################################################

# These variables are used internally in the make process

mdir    := $(shell pwd)
sdir    := $(mdir)/src
adir    := $(mdir)/aux
cdir    := $(mdir)/config


# To whom mail with bug reports and crash messages will get send

MAIL_ADDRESS   = Jens.Toerring@physik.fu-berlin.de


# Set up a variable with the machines FQDN

machine_name = $(shell hostname -f)


# Check that the machine has a new libc, i.e. at least version 2, otherwise
# some problems with the older libc have to be dealt with in the program

ifneq ($(shell ls /lib/libc.so.6),/lib/libc.so.6)
	CFLAGS += -DIS_STILL_LIBC1
endif


# Pass variables via macros to the source files
# For directories make sure they end with a `/'.

ifdef bindir
	CFLAGS += -Dbindir=\"$(bindir:/=)/\"
else
	CFLAGS += -Dbindir=\"$(mdir)/bin/\"
endif

ifdef libdir
	CFLAGS += -Dlibdir=\"$(libdir:/=)/\"
else
	CFLAGS += -Dlibdir=\"$(mdir)/lib/fsc2/\"
endif

ifdef auxdir
	CFLAGS += -Dauxdir=\"$(auxdir:/=)/\" -I$(adir)
else
	CFLAGS += -Dauxdir=\"$(mdir)/lib/fsc2/aux/\" -I$(adir)
endif

ifdef docdir
	CFLAGS += -Ddocdir=\"$(docdir:/=)/\"
else
	CFLAGS += -Ddocdir=\"$(mdir)/share/doc/\"
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

ifdef MAIL_ADDRESS
	CFLAGS += -DMAIL_ADDRESS=\"$(MAIL_ADDRESS)\"
endif

ifndef OWNER
	OWNER = fsc2
endif 

ifndef GROUP
	GROUP = fsc2
endif 

ifdef NUM_SERIAL_PORTS
	CFLAGS += -DNUM_SERIAL_PORTS=$(NUM_SERIAL_PORTS)
else
	CFLAGS += -DNUM_SERIAL_PORTS=0
endif

ifdef IS_I386
	CFLAGS += -DIS_I386
endif


# Define variables with full paths for the 'mail' and 'addr2line' programs
# and make sure the mail program and addr2line got found by 'which', otherwise
# don't set the variables

MAIL_PROGRAM   = $(shell which mail)
ADDR2LINE      = $(shell which addr2line)

ifneq ($(word 1,$(MAIL_PROGRAM)),which:)
	CFLAGS += -DMAIL_PROGRAM=\"$(MAIL_PROGRAM)\"
endif

ifneq ($(word 1,$(ADDR2LINE)),which:)
	CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"
endif


# Here's some stuff that makes it more convenient to make and install on
# machines that I know well enough - for these machines I don't have
# to edit the Makefile all the time...

INSTALL_MACHINE   = install-unknown
UNINSTALL_MACHINE = uninstall-unknown

ifeq ($(machine_name),pion.anorg.chemie.uni-frankfurt.de)
	GPIB_LIBRARY      = LLP
	INSTALL_MACHINE   = install-$(machine_name)
	UNINSTALL_MACHINE = uninstall-$(machine_name)
endif

ifeq ($(machine_name),moebi.physik.fu-berlin.de)
	GPIB_LIBRARY      = JTT
	INSTALL_MACHINE   = install-$(machine_name)
	UNINSTALL_MACHINE = uninstall-$(machine_name)
endif

ifeq ($(machine_name),crowley.physik.fu-berlin.de)
	GPIB_LIBRARY      = JTT
	INSTALL_MACHINE   = install-$(machine_name)
	UNINSTALL_MACHINE = uninstall-$(machine_name)
endif

ifeq ($(machine_name),door.physik.fu-berlin.de)
	GPIB_LIBRARY      = JTT
	INSTALL_MACHINE   = install-$(machine_name)
	UNINSTALL_MACHINE = uninstall-$(machine_name)
endif

ifeq ($(machine_name),isaak.physik.fu-berlin.de)
	GPIB_LIBRARY      = JTT
	INSTALL_MACHINE   = install-$(machine_name)
	UNINSTALL_MACHINE = uninstall-$(machine_name)
endif

ifeq ($(machine_name),crowley.none)
	GPIB_LIBRARY      = JTT
	INSTALL_MACHINE   = install-$(machine_name)
	UNINSTALL_MACHINE = uninstall-$(machine_name)
endif


# Finally the definition of further needed variables

ifeq ($(GPIB_LIBRARY),LLP)
	GPIB_LIBRARY = old
	LIBS += -lgpib
else
	ifeq ($(GPIB_LIBRARY),NI)
		GPIB_LIBRARY = ni
		LIBS += -lgpib
		ifdef GPIB_CONF_FILE
			CFLAGS += -DGPIB_CONF_FILE=\"$(GPIB_CONF_FILE)\"
		else
			CFLAGS += -DGPIB_CONF_FILE=\"/etc/gpib.conf\"
		endif
	else
		ifeq ($(GPIB_LIBRARY),JTT)
			GPIB_LIBRARY = jtt
			LIBS += -lgpib
		else
			GPIB_LIBRARY = none
		endif
	endif
endif


export            # export all variables to sub-makes


############## End of configuration section ##############


# Switch off all implicit rules

.SUFFIXES:
.PHONY: all install-strip install uninstall clean distclean pack ZIP


# Default rule: make the main program, the primary lexer and the modules
# (the Makefile for modules is called via the Makefile in src)

all:
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
	$(MAKE) -C utils install
	$(MAKE) -C scripts install
	$(MAKE) -C doc install


uninstall:
	-$(RM) $(RMFLAGS) $(libdir)/Functions
	-$(RM) $(RMFLAGS) $(libdir)/Devices
	-for file in `basename $(adir)/*.xpm`; do  \
		$(RM) $(RMFLAGS) $(auxdir)/$$file;     \
	done
	$(MAKE) -C src uninstall
	$(MAKE) -C modules uninstall
	$(MAKE) -C utils uninstall
	$(MAKE) -C scripts uninstall
	$(MAKE) -C doc uninstall
	if [ -z "`ls -a $(bindir)`" ]; then  \
		rmdir $(bindir);                 \
	fi
	if [ -z "`ls -a $(libdir)`" ]; then  \
		rmdir $(libdir);                 \
	fi

# `clean' deletes all the files remaining from the compilation while
# `distclean' also removes all executables, shared libraries and documentation

clean:
	$(MAKE) -C src clean
	$(MAKE) -C modules clean
	$(MAKE) -C doc clean

distclean:
	$(MAKE) -C src distclean
	$(MAKE) -C modules distclean
	$(MAKE) -C doc distclean
	$(MAKE) -C utils distclean
	-$(RM) $(RMFLAGS) *~


# Clean up everything for a distribution and create a zipped tarball

pack:
	$(MAKE) distclean
	cd ..; tar -c fsc2 | gzip -c -9 > fsc2.tgz


# Create zipped tarball and write it to the ZIP drive (at home or on isaak
# also mount and unmount the ZIP drive otherwise assume that it's already
# mounted)

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
