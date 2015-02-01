.PHONY: all
all:

-include $(wildcard build/*.d)

OBJECTS = build/main.o build/load_image.o build/util.o build/individual.o

C_FLAGS += -Ibuild -Isrc -g -Wall -Werror
COMPILE_C = clang -c -o $@ -MMD -MP -MF $@.d $(C_FLAGS) $<

build/legend-of-swarkland: $(OBJECTS)
	clang -o $@ -lSDL2 -lrucksack -lfreeimage $(OBJECTS)
all: build/legend-of-swarkland

build/%.o: src/%.c
	$(COMPILE_C)

$(OBJECTS): | build
build:
	mkdir -p $@

build/resources.bundle:
	rucksack bundle assets.json $@ --deps $@.d
all: build/resources.bundle

.PHONY: clean
clean:
	rm -rf build
