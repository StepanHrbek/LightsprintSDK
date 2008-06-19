RR_IO_PATH = ../../src/LightsprintIO
RR_GL_PATH = ../../src/LightsprintGL

all:
	@if [ -e $(RR_IO_PATH) ] ; then \
		cd $(RR_IO_PATH) && make; \
	fi
	@if [ -e $(RR_GL_PATH) ] ; then \
		cd $(RR_GL_PATH) && make; \
	fi
	@make -f makefile.proj

%:
	@make -f makefile.proj $@
