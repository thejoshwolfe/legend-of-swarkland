.PHONY: all
all:

-include $(wildcard build/*.d)

C_FLAGS += -Ibuild -Isrc -g -Wall -Werror
COMPILE_C = clang -c -o $@ -MMD -MP -MF $@.d $(C_FLAGS) $<

build/legend-of-swarkland: build/main.o
	clang -o $@ $< -lSDL2 -lrucksack -lfreeimage
all: build/legend-of-swarkland

build/%.o: src/%.c
	$(COMPILE_C)

build/main.o: | build

build:
	mkdir -p $@

build/resources.bundle:
	rucksack bundle assets.json $@ --deps $@.d
all: build/resources.bundle

.PHONY: clean
clean:
	rm -rf build
