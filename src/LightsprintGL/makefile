RR_CORE_PATH = ../LightsprintCore

all:
	@if [ -e $(RR_CORE_PATH) ] ; then \
		cd $(RR_CORE_PATH) && make; \
	fi
	@make -f makefile.proj

%:
	@make -f makefile.proj $@
