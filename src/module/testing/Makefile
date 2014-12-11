CC=gcc
FLAGS=-std=c99 -Wall -Wextra -Werror -D_XOPEN_SOURCE
CFLAGS=$(FLAGS) -g
LDFLAGS=
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

#if it doesn't work, put there the name of your Main file
#EXEC=$(shell grep -H  main *.c | awk {'print $1'} | cut --delimiter='.' -f1)
EXEC=main


all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY : clean distclean run

run:
	./$(EXEC)

clean :
	rm -rf *.o

distclean : clean
	rm -rf $(EXEC)
