all: module/moca.ko pintool/moca.so libs/replace_malloc.so

module/moca.ko: libs/replace_malloc.so
	cd module; make

pintool/moca.so:
	make -C pintool

libs/replace_malloc.so:
	make -C libs

.PHONY: clean-module clean-pintool clean

clean-module:
	make clean -C module

clean-pintool:
	make clean -C pintool

clean-libs:
	make clean -C libs

clean: clean-module clean-pintool clean-libs
