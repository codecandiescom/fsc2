# $Id$

# list of all scripts for the current machine (i.e. all scripts whose
# extension is equal to the machines FQDN)

scripts = $(wildcard *.$(whom_for))


# install these scripts in the bindir (but without the FQDN extension)
# Some care has to be taken for the case that there are no scripts...

install: $(scripts)
	if [ -n "$(scripts)" ]; then \
		make install_them;       \
	fi


install_them:
	for file in $(scripts); do \
		install -m 755 -o fsc2 -g fsc2 \
			$$file $(bindir)/$${file%.$(whom_for)}; \
	done
