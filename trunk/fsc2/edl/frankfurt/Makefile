EDL           := $(wildcard *.EDL)
MACHINE       := zethossuse.prisner.de
ALL           := $(patsubst %.EDL,%,$(EDL)) epr shutdown shutdown_zero pfail_recover

all: $(patsubst %.EDL,%,$(EDL))

%: %.EDL
	@if [ ! -e $@ -o $< -nt $@ ]; then  \
		echo "fsc2_guify $< $@";        \
		fsc2_guify $< $@;               \
	fi

bla:
	@for f in $(ALL); do       \
		echo "cp $$f ../../scripts/$$f.$(MACHINE)";    \
		done

install:
	@for f in $(ALL); do                                      \
		if [    ! -e ../../scripts/$$f.$(MACHINE)             \
			 -o $$f -nt ../../scripts/$$f.$(MACHINE) ]; then  \
			echo "cp $$f ../../scripts/$$f.$(MACHINE)";       \
	    	cp $$f ../../scripts/$$f.$(MACHINE);              \
		fi;                                                   \
	done

clean:
	rm -f $(patsubst %.EDL,%,$(EDL)) *~
