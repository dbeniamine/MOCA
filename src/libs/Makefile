CC=gcc

all: replace_malloc.so

%.so: %.c
	$(CC) -shared -fPIC $^ -o $@ -ldl

.PHONY: clean

clean:
	rm -f *.so
