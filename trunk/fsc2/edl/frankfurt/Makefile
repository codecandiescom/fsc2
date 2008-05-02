EDL           := $(wildcard *.EDL)
MACHINE       := zethossuse.prisner.de
ALL           := $(patsubst %.EDL,%,$(EDL)) epr monitor_mode fsps25_shutdown fsps25_shutdown_zero pfail_recover

all: $(patsubst %.EDL,%,$(EDL))

%: %.EDL
	@if [ ! -e $@ -o $< -nt $@ ]; then  \
		echo "fsc2_guify $< $@";        \
		fsc2_guify $< $@;               \
	fi

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
