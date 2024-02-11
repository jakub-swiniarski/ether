SOURCES=$(wildcard src/*.c)
HEADERS=$(wildcard src/*.h)
OBJECTS=$(patsubst src/%.c,%.o,$(SOURCES))

ether: $(OBJECTS)
	gcc -o $@ $(OBJECTS)

$(OBJECTS): $(SOURCES) $(HEADERS)
	gcc -c $(SOURCES) -O2

.PHONY: clean run install uninstall

clean:
	rm *.o ether

run: ether
	./ether

install: ether res
	cp -f ether /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/ether
