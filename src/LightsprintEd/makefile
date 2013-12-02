RR_CORE_PATH = ../LightsprintCore
RR_GL_PATH = ../LightsprintGL

all:
	@if [ -e $(RR_CORE_PATH) ] ; then \
		cd $(RR_CORE_PATH) && make; \
	fi
	@if [ -e $(RR_GL_PATH) ] ; then \
		cd $(RR_GL_PATH) && make; \
	fi
	@make -f makefile.proj

%:
	@make -f makefile.proj $@
