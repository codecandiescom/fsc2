# $Id$


docs  = $(wildcard *.texi)


all:
	$(MAKE) man
	$(MAKE) info
	$(MAKE) html
	$(MAKE) ps
	$(MAKE) pdf


man: fsc2.1.gz

fsc2.1.gz: fsc2.1
	sed 's/DOCDIR/$(subst /,\/,$(docdir))/' fsc2.1 | gzip -9 -c > fsc2.1.gz


info: fsc2.info

fsc2.info: $(docs)
	makeinfo fsc2.texi


html: fsc2.html

fsc2.html: $(docs)
	./texi2html -frames -split chapter -menu -number -sec_nav -iso fsc2.texi
	sed -e 's/fsc2_[0-9]\+/fsc2/' fsc2_frame.html > fsc2_frame.x
	mv fsc2_frame.x fsc2_frame.html


ps: fsc2.ps

fsc2.ps: $(docs)
	if [ "`eval texi2dvi --help 2>/dev/null`" -a   \
		 "`eval dvips --help 2>/dev/null`" ]; then \
		texi2dvi -c fsc2.texi; \
		dvips fsc2.dvi;        \
	fi


pdf: fsc2.pdf

fsc2.pdf: $(docs)
	if [ "`eval texi2dvi --help 2>/dev/null`" -a     \
		 "`eval pdftex --help 2>/dev/null` " ]; then \
		texi2dvi --pdf -c fsc2.texi; \
	fi


clean:
	$(RM) $(RMFLAGS) fsc2.dvi *.log


install:
	$(INSTALL) -m 444 fsc2.1.gz /usr/local/man/man1
	$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2*.html $(docdir)
	if [ -f fsc2.ps ]; then \
		$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2.ps $(docdir); \
	fi
	if [ -f fsc2.pdf ]; then \
		$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2.pdf $(docdir); \
	fi
	install-info --delete fsc2 $(infodir)/dir
	$(RM) $(RMFLAGS) $(infodir)/fsc2.info*.gz
	$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2.info* $(infodir)
	gzip -9 -f $(infodir)/fsc2.info*
	install-info $(infodir)/fsc2.info $(infodir)/dir


distclean:
	$(RM) $(RMFLAGS) fsc2*.html fsc2_*.html fsc2.ps fsc2.pdf *.info* *~ *.gz
