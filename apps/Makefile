all:
	$(MAKE) -C .. applications

clean:
	$(MAKE) -C .. clean_apps

# build any target by forwarding to the main makefile
% : forward_ 
	@echo the call is for $@
	$(MAKE) -C .. apps/$@

.PHONY : forward_ clean

