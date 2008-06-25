RR_IO_PATH = ../../src/LightsprintIO

all:
	@if [ -e $(RR_IO_PATH) ] ; then \
		cd $(RR_IO_PATH) && make; \
	fi
	@make -f makefile.proj

%:
	@make -f makefile.proj $@
