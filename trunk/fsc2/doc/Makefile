# $Id$


html: $(wildcard fsc2_*.texi)
	texi2html -split_node -menu -number -expandinfo -glossary -acc fsc2.texi


ps: $(wildcard fsc2_*.texi)
	texi2dvi -c fsc2.texi
	dvips fsc2.dvi


clean:
	$(RM) $(RMFLAGS) fsc2.dvi

distclean:
	$(RM) $(RMFLAGS) fsc2_*.html fsc2.ps *~
