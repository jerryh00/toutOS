CPPFLAGS += -nostdinc -I ../include --include ../include/arch.h
CFLAGS += -Wall -g -Werror -nostdlib
CXXFLAGS += -Wall -g -Werror
LDFLAGS += -lcppunit -ldl

SRC := tests_test_main.cpp StringTest.cpp
OBJS :=

.PHONY: all
all: clean tests_test_main
	./tests_test_main

tests_test_main: ${SRC} ${OBJS}
	$(CXX) ${CXXFLAGS} ${SRC} ${LDFLAGS} ${OBJS} -o $@

.c.o:
	$(CC) -c ${CPPFLAGS} ${CFLAGS} $^ -o $@

clean:
	rm -f ../*/*.o ../*.o *.o ../*/*.o tests_test_main
