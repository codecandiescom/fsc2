# $Id$


all: edl-mod.elc

edl-mod.elc: edl-mod.el
	emacs -batch -f batch-byte-compile edl-mod.el

install:
	install -d /usr/share/emacs/site-lisp
	install -m 755 -o fsc2 -g fsc2 edl-mod.elc /usr/share/emacs/site-lisp

distclean:
	$(RM) $(RMFLAGS) edl-mod.elc
