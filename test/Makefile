INSTALL_DIR=~/Work/MemMap
MEMMAP=sudo ../src/utils/MemMap.sh -d $(INSTALL_DIR)
SIZE=20
SEED=42
ALGO=par_modulo
THREADS=4
MAT_ARG="-S $(SIZE) -s $(SEED) -a $(ALGO) -n $(THREADS)"

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
