bin=inspector

# Set the following to '0' to disable log messages:
debug=1

CFLAGS += -Wall -g
LDFLAGS +=

src=inspector.c str_func.c
obj=$(src:.c=.o)

$(bin): $(obj)
	$(CC) $(CFLAGS) $(LDFLAGS) -DDEBUG=$(debug) $(obj) -o $@

inspector.o: inspector.c str_func.h str_func.c
str_func.o: str_func.h str_func.c

clean:
	rm -f inspector $(obj)


# Tests --

test: inspector ./tests/run_tests
	./tests/run_tests $(run)

testupdate: testclean test

./tests/run_tests:
	git submodule update --init --remote

testclean:
	rm -rf tests
