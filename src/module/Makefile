obj-m := moca.o
moca-objs := moca_main.o  moca_hashmap.o moca_false_pf.o moca_tasks.o moca_taskdata.o moca_page.o moca_probes.o
ccflags-m += -Wall -Werror

all: module

module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

.PHONY: clean

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f tags cscope*

load: install
	sudo modprobe moca

unload:
	sudo rmmod moca

install: module
	sudo make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
	sudo depmod -A
