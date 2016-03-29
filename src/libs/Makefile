CC=gcc

all: moca.so

%.so: %.c
	$(CC) -D__LIBNAME__='"$(PWD)/$@"' -shared -fPIC $^ -g -o $@ -ldl -lelf

.PHONY: clean

clean:
	rm -f *.so
