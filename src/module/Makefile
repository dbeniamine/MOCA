obj-m := memmap.o
memmap-objs := memmap_main.o memmap_tlb.o memmap_threads.o memmap_probes.o
ccflags-m += -Wall -Werror

all: module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load: install
	sudo modprobe memmap

unload:
	sudo rmmod memmap

install: module
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra/memmap/
	sudo mv ./memmap.ko /lib/modules/$(shell uname -r)/extra/memmap/
	sudo depmod -a
