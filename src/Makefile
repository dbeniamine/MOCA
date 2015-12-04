all: module/moca.ko pintool/moca.so

module/moca.ko:
	cd module; make

pintool/moca.so:
	make -C pintool

.PHONY: clean-module clean-pintool clean

clean-module:
	make clean -C module

clean-pintool:
	make clean -C pintool

clean: clean-module clean-pintool
