all: libhook.so.1.0.0 test_main

libhook.so.1.0.0: hook_main.c utils.c thread.c
	$(CC) -fPIC $^ -shared -I. -Wl,-soname,libhook.so.1 -o $@

test_main: test_main.c
	$(CC) -lpthread -o $@ $^

run: libhook.so.1.0.0 test_main
	LD_PRELOAD=./libhook.so.1.0.0 ./test_main