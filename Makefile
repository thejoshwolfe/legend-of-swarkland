.PHONY: all
all:

build/legend-of-swarkland: build/main.o
	clang -o $@ $<
all: build/legend-of-swarkland

build/main.o: src/main.c
	clang -o $@ -c $<

build/main.o: | build

build:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf build
