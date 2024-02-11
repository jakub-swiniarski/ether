SOURCES=$(wildcard *.c)
HEADERS=$(wildcard *.h)
OBJECTS=$(SOURCES:.c=.o)

ether: $(OBJECTS)
	gcc -o $@ $(OBJECTS)

$(OBJECTS): $(SOURCES) $(HEADERS)
	gcc -c $(SOURCES) -std=c99 -pedantic -Wall -Wextra -O2

.PHONY: clean run install uninstall

clean:
	rm *.o ether

run: ether
	./ether

install: ether res
	cp -f ether /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/ether
