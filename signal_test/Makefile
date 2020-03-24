# -fsanitize=address -fsanitize-recover=address
all: libhook.so.1.0.0 test_main

libhook.so.1.0.0: hook_main.c utils.c thread.c object.c rbtree.c breakpoint.c
	$(CC) -g -fPIC $^ -shared -I. -Wl,-soname,libhook.so.1 -o $@

test_main: test_main.c
	$(CC) -g -lpthread -o $@ $^

run: libhook.so.1.0.0 test_main
	LD_PRELOAD=./libhook.so.1.0.0 ./test_main

clean:
	rm libhook.so.1.0.0 test_main

.PHONY: run clean