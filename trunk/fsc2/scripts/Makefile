# $Id$

all:
	$(MAKE) install-$(shell hostname -f)


install-pion.anorg.chemie.uni-frankfurt.de: ;

install-moebi.physik.fu-berlin.de:
	$(RM) $(RMFLAGS) $(bindir)/cw_epr
	install -m 755 -o fsc2 -g fsc2 cw_epr.moebi.physik.fu-berlin.de \
			$(bindir)/cw_epr


install-crowley.physik.fu-berlin.de:
	$(RM) $(RMFLAGS) $(bindir)/cw_endor
	install -m 755 -o fsc2 -g fsc2 cw_endor.crowley.physik.fu-berlin.de \
			$(bindir)/cw_endor


install-crowley.none: ;
