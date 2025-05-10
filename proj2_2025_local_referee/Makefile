#COMPILER ?= mpicc
COMPILER ?= mpicc

CFLAGS ?= -O2 -g -Wall -Wno-variadic-macros -pedantic -DDEBUG $(GCC_SUPPFLAGS)
LDFLAGS ?= -g
LDLIBS =

EXECUTABLE = player/myplayer

SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:src/%.c=player/%.o)

all: release

release: $(OBJS)
	$(COMPILER) $(LDFLAGS) -o $(EXECUTABLE) $(OBJS) $(LDLIBS)

player/%.o: src/%.c | player
	$(COMPILER) $(CFLAGS) -o $@ -c $<

player:
	mkdir -p $@

clean:
	rm -f player/*.o
	rm -f ${EXECUTABLE}

cleandata:
	rm -f log*.txt
	rm -f my_player.log
