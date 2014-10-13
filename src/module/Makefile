obj-m := memmap.o
memmap-objs := memmap_main.o memmap_tlb.o memmap_threads.o memmap_probes.o
ccflags-m += -Wall -Werror

all: module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

load: module
	sudo insmod ./memmap.ko

unload:
	sudo rmmod memmap
