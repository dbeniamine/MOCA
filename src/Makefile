all: module/moca.ko libs/replace_malloc.so

module/moca.ko: libs/replace_malloc.so
	cd module; make

libs/replace_malloc.so:
	make -C libs

.PHONY: clean-module clean

clean-module:
	make clean -C module

clean-libs:
	make clean -C libs

clean: clean-module clean-libs
