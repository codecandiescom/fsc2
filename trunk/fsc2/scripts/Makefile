# $Id$

# list of all scripts for the current machine (i.e. all scripts whose
# extension is equal to the machines FQDN)

scripts = $(wildcard *.$(shell hostname -f))


# install these scripts in the bindir (but without the FQDN extension)

install: $(scripts)
	for file in $(scripts); \
	do \
		install -m 755 -o fsc2 -g fsc2 \
			$$file $(bindir)/$${file%.$(shell hostname -f)}; \
	done
