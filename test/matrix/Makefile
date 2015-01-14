CC=gcc
DEBUG=yes
FLAGS=-std=gnu99 -Wall -Wextra -Werror
ifeq ($(DEBUG),yes)
	CFLAGS=$(FLAGS) -g
else
	CFLAGS=$(FLAGS) -O2
endif
LDFLAGS=-lpthread -lrt
OBJ=matrix_naif.c
EXEC=matrix


all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

.PHONY : clean mrproper

clean :
	rm -rf *.o

distclean : clean
	rm -rf $(EXEC)
