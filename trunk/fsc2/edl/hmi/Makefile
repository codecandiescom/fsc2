EDL      := $(wildcard *.EDL)


all: $(patsubst %.EDL,%,$(EDL))

%: %.EDL
	../../utils/fsc2_guify $< $@

set_resonators:
	./set_resonators ER023M monitor_bruker.EDL > monitor_bruker.EDL
	./set_resonators ER023M cw_epr_bruker.EDL > cw_epr_bruker.EDL
	./set_resonators SR830 monitor_sr.EDL > monitor_sr.EDL
	./set_resonators SR830 cw_epr_sr.EDL > cw_epr_sr.EDL

install:
	@for f in $(scripts); do       \
		if [    $$f -nt ../../scripts/$$f.aiesr6.hmi-domain       \
			 -o ! -e ../../scripts/$$f.aiesr6.hmi-domain ]; then  \
			echo "cp $$f ../../scripts/$$f.aiesr6.hmi-domain";    \
	    	cp $$f ../../scripts/$$f.aiesr6.hmi-domain;           \
		fi;                                                       \
	done

clean:
	rm -f $(patsubst %.EDL,%,$(EDL)) *~
