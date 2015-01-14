obj-m := memmap.o
memmap-objs := memmap_main.o  memmap_hashmap.o memmap_tasks.o memmap_taskdata.o memmap_page.o memmap_threads.o memmap_probes.o
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
	sudo make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
	sudo depmod -A
