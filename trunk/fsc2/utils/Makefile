# $Id$
#
# Copyright (c) 2001 Jens Thoms Toerring
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
# along with GNU Emacs; see the file COPYING.  If not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


all: edl-mod.elc

edl-mod.elc: edl-mod.el
	emacs -batch -f batch-byte-compile edl-mod.el

install:
	$(INSTALL) -d /usr/share/emacs/site-lisp
	$(INSTALL) -m 644 edl-mod.el /usr/share/emacs/site-lisp
	$(INSTALL) -m 444 edl-mod.elc /usr/share/emacs/site-lisp

distclean:
	$(RM) $(RMFLAGS) edl-mod.elc
