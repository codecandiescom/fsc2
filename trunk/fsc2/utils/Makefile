# $Id$


all: edl-mod.elc

edl-mod.elc: edl-mod.el
	emacs -batch -f batch-byte-compile edl-mod.el

install:
	install -d /usr/share/emacs/site-lisp
	install -m 644 edl-mod.el /usr/share/emacs/site-lisp
	install -m 444 edl-mod.elc /usr/share/emacs/site-lisp

distclean:
	$(RM) $(RMFLAGS) edl-mod.elc
