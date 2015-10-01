CC=gcc
CFLAGS= -g -Wall -Wextra -D_GNU_SOURCE #XOPEN_SOURCE=500
LDFLAGS=-lpthread
SRC=$(wildcard *.c)
DEPS=$(wildcard *.h)
OBJ=$(SRC:.c=.o)

#if it doesn't work, put there the name of your Main file
EXEC=simulator


all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(DEPS)
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY : clean distclean run

run:
	./$(EXEC)

clean :
	rm -rf *.o

distclean : clean
	rm -rf $(EXEC)
