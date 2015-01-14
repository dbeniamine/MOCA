CC=gcc
DEBUG=yes
#FLAGS=-std=c99 -Wall -Wextra -Werror -D_XOPEN_SOURCE
FLAGS=-std=c99 -Wall -Wextra -D_XOPEN_SOURCE
ifeq ($(DEBUG),yes)
	CFLAGS=$(FLAGS) -g
else
	CFLAGS=$(FLAGS) -O2
endif
LDFLAGS=-lpthread
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

#if it doesn't work, put there the name of your Main file
EXEC=$(shell grep -H  main *.c | awk {'print $1'} | cut --delimiter='.' -f1)


all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY : clean mrproper

clean :
	rm -rf *.o

mrproper : clean
	rm -rf $(EXEC)
