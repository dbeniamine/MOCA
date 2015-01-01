# MemMap config
INSTALL_DIR=/home/david/Work/MemMap
LOG_FILE=$(INSTALL_DIR)/log
MEMMAP=sudo ../src/utils/MemMap -d $(INSTALL_DIR) -p $(PRIO) -f $(LOG_FILE) \
	   -w 50 -C 20 -F 2
#-b 14 -F 2
# Testing config
PRIO=99
SIZE=500
SEED=$(shell head /dev/urandom -n 1 | tr -cd '[:digit:]' | sed 's/\([0-9]\{1,4\}\).*/\1/')
ALGO=par_modulo
THREADS=2
MAT_ARG="-S $(SIZE) -s $(SEED) -a $(ALGO) -n $(THREADS) -v -V"

all: fork matrix

fork:
	$(MEMMAP) -c ./fork.sh

matrix: matrix/matrix_naif
	$(MEMMAP) -c matrix/matrix_naif -a $(MAT_ARG)

matrix/matrix_naif: matrix/matrix_naif.c
	make -C matrix/

.phony: clean

clean:
	make clean -C matrix
