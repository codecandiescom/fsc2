# $Id$


docs           = $(wildcard fsc2_*.texi)


html: $(docs)
	texi2html -split_node -menu -number -expandinfo -glossary -acc fsc2.texi


ps: $(docs)
	texi2dvi -c fsc2.texi
	dvips fsc2.dvi

pdf: $(docs)
	texi2dvi --pdf -c fsc2.texi

clean:
	$(RM) $(RMFLAGS) fsc2.dvi

distclean:
	$(RM) $(RMFLAGS) fsc2_*.html fsc2.ps fsc2.pdf *~
