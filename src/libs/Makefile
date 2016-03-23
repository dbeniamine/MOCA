CC=gcc

all: replace_malloc.so

%.so: %.c
	$(CC) -D__LIBNAME__='"$(PWD)/$@"' -shared -fPIC $^ -g -o $@ -ldl -lelf -lpthread

.PHONY: clean

clean:
	rm -f *.so
