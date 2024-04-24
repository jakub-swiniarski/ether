SRC = $(wildcard *.c)
HDR = $(wildcard *.h)
OBJ = $(SRC:.c=.o)

all: ether

%.o: %.c
	gcc -c -std=c99 -pedantic -Wall -Wextra -O2 $<

$(OBJ): $(HDR)

ether: $(OBJ)
	gcc -o $@ $(OBJ)

run: all
	./ether

clean:
	rm -f *.o ether

install: all
	cp -f ether /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/ether

.PHONY: all run clean install uninstall
