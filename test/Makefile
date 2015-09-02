# Moca config
INSTALL_DIR=$(HOME)/Work/Moca
BENCHDIR=$(HOME)/Work/Bench
MOCA=sudo ../src/utils/moca -d $(INSTALL_DIR) $(PRIO) -w $(INTERVAL) -C $(NB_CHUNKS)
#-b 14 -F 2
# Testing config
#PRIO=-p 99
SIZE=1500
INTERVAL=40
NB_CHUNKS=30
LOGDIR=$(PWD)
#SEED=$(shell head /dev/urandom | tr -cd '[:digit:]' | sed 's/\([0-9]\{1,4\}\).*/\1/')
SEED=1741
ALGO=par_modulo
THREADS=2
MAT_ARG="-S $(SIZE) -s $(SEED) -a $(ALGO) -n $(THREADS) $(VERIF)"

all: fork matrix

fork:
	$(MOCA) -c ./fork.sh

matrix: $(BENCHDIR)/matrix/matrix
	$(MOCA) -c $(BENCHDIR)/matrix/matrix -a $(MAT_ARG)

$(BENCHDIR)/matrix/matrix: $(BENCHDIR)/matrix/matrix_naif.c
	$(MAKE) -C $(BENCHDIR)/matrix/

.phony: clean

clean:
	make clean -C $(BENCHDIR)/matrix
