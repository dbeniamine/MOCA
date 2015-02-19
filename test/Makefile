# Moca config
INSTALL_DIR=/home/david/Work/Moca
LOG_FILE=$(INSTALL_DIR)/log
MOCA=sudo ../src/utils/moca -d $(INSTALL_DIR) -p $(PRIO) -f $(LOG_FILE) \
	   -w $(INTERVAL) -C $(NB_CHUNKS) $(COMPI) -D $(LOGDIR)
#-b 14 -F 2
# Testing config
PRIO=99
SIZE=1000
INTERVAL=50
NB_CHUNKS=20
COMPI=
LOGDIR=$(PWD)
#SEED=$(shell head /dev/urandom | tr -cd '[:digit:]' | sed 's/\([0-9]\{1,4\}\).*/\1/')
SEED=1550
ALGO=par_modulo
THREADS=2
VERIF=-v -V
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
