RR_IO_PATH = ../../src/LightsprintIO
RR_ED_PATH = ../../src/LightsprintEd

all:
	@if [ -e $(RR_IO_PATH) ] ; then \
		cd $(RR_IO_PATH) && make; \
	fi
	@if [ -e $(RR_ED_PATH) ] ; then \
		cd $(RR_ED_PATH) && make; \
	fi
	@make -f makefile.proj

%:
	@make -f makefile.proj $@
