# $Id$


docs  = $(wildcard *.texi)


all:
	$(MAKE) info
	$(MAKE) html
	$(MAKE) ps
	$(MAKE) pdf


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
	if `which texi2dvi`; then \
		texi2dvi -c fsc2.texi; \
		dvips fsc2.dvi; \
	fi

pdf: fsc2.pdf

fsc2.pdf: $(docs)
	if `which texi2dvi`; then \
		texi2dvi --pdf -c fsc2.texi; \
	fi

clean:
	$(RM) $(RMFLAGS) fsc2.dvi *.log

install:
	$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2*.html $(docdir)
	$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2.ps $(docdir)
	$(INSTALL) -m 444 -o fsc2 -g fsc2 fsc2.pdf $(docdir)

distclean:
	$(RM) $(RMFLAGS) fsc2*.html fsc2_*.html fsc2.ps fsc2.pdf *.info* *~
