all: .phony

.phony:
	cd LightsprintCore && make
	cd LightsprintIO && make
	cd LightsprintGL && make

clean:
	cd LightsprintCore && make clean
	cd LightsprintIO && make clean
	cd LightsprintGL && make clean

%:
	cd LightsprintCore && make $@
	cd LightsprintIO && make $@
	cd LightsprintGL && make $@
