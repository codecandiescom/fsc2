# Makefile for ME-6000 and ME-6100 driver and library
#
# Copyright (C) 2002-2011 Jens Thoms Toerring
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
# To contact the author send email to:  jt@toerring.de


SHELL   := /bin/sh
CC      := gcc
INSTALL := install
RM      := -rm
RMFLAGS := -f

prefix  = $(shell if [ -e driver/autoconf.h ]; then           \
			          grep ME6X00_PREFIX driver/autoconf.h |  \
	                  sed 's/[^\"]*//' | tr -d \";            \
	              else                                        \
	                  echo "/usr/local";                      \
	              fi)

export


.SUFFIXES:
.PHONY: all driver lib install cleanup clean


all:
	$(MAKE) driver
	$(MAKE) lib


driver:
	@if [ ! -e driver/autoconf.h ]; then                        \
		echo "**********************************************";  \
		echo "*   The file driver/autoconf.h is missing.   *";  \
		echo "*    Please run './INSTALL' to create it.    *";  \
		echo "**********************************************";  \
		exit 2;                                                 \
	fi
	$(MAKE) -C driver


lib:
	$(MAKE) -C lib


install:
	$(MAKE) -C driver install
	$(MAKE) -C lib install


cleanup:
	$(MAKE) -C driver cleanup
	$(MAKE) -C lib cleanup
	$(RM) $(RMFLAGS) makelog svn-* *~


clean:
	$(MAKE) -C driver clean
	$(MAKE) -C lib clean
	$(RM) $(RMFLAGS) driver/autoconf.h makelog svn-* *~
