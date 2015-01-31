all: build/legend-of-swarkland

clean:
	rm -rf build

build/legend-of-swarkland: build/legend-of-swarkland.o
	clang -o $@ $<

build/legend-of-swarkland.o: src/main.c
	clang -o $@ -c $<

build/legend-of-swarkland.o: |build

build:
	mkdir -p $@
