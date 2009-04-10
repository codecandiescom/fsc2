#  $Id$
#
#  Copyright (C) 1999-2009 Jens Thoms Toerring
#
#  This file is part of fsc2.
#
#  Fsc2 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  Fsc2 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with GNU Emacs; see the file COPYING.  If not, write to the
#  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.


emacs_dir := $(shell if [ -d /usr/local/share/emacs/site-lisp ]; then  \
						 echo /usr/local/share/emacs/site-lisp;        \
                     else if [ -d /usr/share/emacs/site-lisp ]; then   \
						     echo /usr/share/emacs/site-lisp;          \
					 	  fi                                           \
					 fi )
vim_dir := $(shell if [ -d /usr/local/share/vim/current ]; then   \
						 echo /usr/local/share/vim/current;       \
                     else if [ -d /usr/share/vim/current ]; then  \
						     echo /usr/share/vim/current;         \
					 	  fi                                      \
					 fi )

all:
	$(MAKE) utils


ifdef WITH_MEDRIVER
utils: edl-mod.elc edl.vim me_dio_get_subdevices
else
utils: edl-mod.elc edl.vim
endif


edl-mod.el: edl-mod.el.template $(sdir)/func.c $(cdir)/Functions
	@sed -e 's/BUILTIN_FUNCTION_LIST/$(shell SRC_DIR=$(sdir) ./get_built_in_functions emacs 2>/dev/null)/;s/DEVICE_FUNCTION_LIST/$(shell CONF_DIR=$(cdir) ./get_device_functions emacs 2>/dev/null)/' $< > $@;


edl-mod.elc: edl-mod.el
	emacs -batch -f batch-byte-compile edl-mod.el;


edl.vim: edl.vim.template $(sdir)/func.c $(cdir)/Functions
	@sed -e 's/BUILTIN_FUNCTION_LIST/$(shell SRC_DIR=$(sdir) ./get_built_in_functions vim)/;s/DEVICE_FUNCTION_LIST/$(shell CONF_DIR=$(cdir) ./get_device_functions vim)/' $< > $@;


ifdef WITH_MEDRIVER
me_dio_get_subdevices: me_dio_get_subdevices.c
	gcc -o $@ $< -I$(medriver_incl_path) -L$(medriver_lib_path) -lmedriver
endif


install: edl-mod.elc
	$(INSTALL) -m 755 fsc2_guify $(bindir)
	$(INSTALL) -m 755 fsc2_pulses $(bindir)
	$(INSTALL) -m 755 epr_modulation $(bindir)
	$(INSTALL) -d $(emacs_dir)
	if [ -n "$(emacs_dir)" ]; then                   \
		$(INSTALL) -m 644 edl-mod.el $(emacs_dir);   \
		$(INSTALL) -m 444 edl-mod.elc $(emacs_dir);  \
	fi
	if [ -n "$(vim_dir)" ]; then                      \
		$(INSTALL) -m 644 edl.vim $(vim_dir)/plugin;  \
	fi
ifdef WITH_HTTP_SERVER
	$(EXEC_INSTALL) -m 755 -o $(OWNER) -g $(GROUP) fsc2_http_server $(bindir)
endif


uninstall:
	-$(RM) $(RMFLAGS) $(bindir)/fsc2_guify
	-$(RM) $(RMFLAGS) $(bindir)/fsc2_pulses
	-$(RM) $(RMFLAGS) $(bindir)/epr_modulation
	-if [ -n "$(emacs_dir)" ]; then                  \
		-$(RM) $(RMFLAGS) $(emacs_dir)/edl-mod.el;   \
		-$(RM) $(RMFLAGS) $(emacs_dir)/edl-mod.elc;  \
		-if [ -z "`ls -A $(emacs_dir)`" ]; then      \
			rmdir $(emacs_dir);                      \
		fi                                           \
	fi
	-if [ -n "$(vim_dir)" ]; then                     \
		-$(RM) $(RMFLAGS) $(vim_dir)/plugin/edl.vim;  \
		-if [ -z "`ls -A $(vim_dir)/plugin`" ]; then  \
			rmdir $(vim_dir)/plugin;                  \
		fi                                            \
	fi
ifdef WITH_HTTP_SERVER
	$(RM) $(RMFLAGS) $(bindir)/fsc2_http_server 
endif


cleanup:
	-$(RM) $(RMFLAGS) svn-* *~


clean:
	$(MAKE) cleanup
	-$(RM) $(RMFLAGS) me_dio_get_subdevices edl-mod.elc edl-mod.el edl.vim
