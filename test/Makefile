# Moca config
INSTALL_DIR=/home/david/Work/Moca
MOCA=sudo ../src/utils/moca -d $(INSTALL_DIR) -p $(PRIO) -w $(INTERVAL) -C $(NB_CHUNKS)
#-b 14 -F 2
# Testing config
PRIO=99
SIZE=1500
INTERVAL=40
NB_CHUNKS=30
LOGDIR=$(PWD)
SEED=$(shell head /dev/urandom | tr -cd '[:digit:]' | sed 's/\([0-9]\{1,4\}\).*/\1/')
ALGO=par_modulo
THREADS=2
MAT_ARG="-S $(SIZE) -s $(SEED) -a $(ALGO) -n $(THREADS) $(VERIF)"

all: fork matrix

fork:
	$(MOCA) -c ./fork.sh

matrix: matrix/matrix
	$(MOCA) -c matrix/matrix -a $(MAT_ARG)

matrix/matrix: matrix/matrix_naif.c
	make -C matrix/

.phony: clean

clean:
	make clean -C matrix
