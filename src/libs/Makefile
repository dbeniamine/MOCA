CC=gcc

all: replace_malloc.so

%.so: %.c
	$(CC) -D__LIBNAME__='"$(PWD)/$@"' -shared -fPIC $^ -o $@ -ldl

.PHONY: clean

clean:
	rm -f *.so
