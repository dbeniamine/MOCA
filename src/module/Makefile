obj-m := memmap.o
memmap-objs := memmap_main.o  memmap_tasks.o memmap_taskdata.o memmap_page.o memmap_threads.o memmap_probes.o
ccflags-m += -Wall -Werror

all: module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f tags cscope*

load: install
	sudo modprobe memmap

unload:
	sudo rmmod memmap

install: module
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra/memmap/
	sudo mv ./memmap.ko /lib/modules/$(shell uname -r)/extra/memmap/
	sudo depmod -a
