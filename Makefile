.PHONY: all
all:

-include $(wildcard build/*.d)

OBJECTS = build/main.o build/swarkland.o build/display.o build/load_image.o build/util.o build/individual.o build/path_finding.o build/map.o build/hashtable.o build/random.o build/decision.o build/tas.o build/byte_buffer.o build/item.o build/input.o build/event.o

CPP_FLAGS += -Ibuild -Isrc -g -Wall -Werror
COMPILE_CPP = clang -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

build/legend-of-swarkland: $(OBJECTS)
	clang -o $@ $(OBJECTS) -lstdc++ -lm -lSDL2 -lSDL2_ttf -lrucksack -lfreeimage
all: build/legend-of-swarkland

build/%.o: src/%.cpp
	$(COMPILE_CPP)

$(OBJECTS): | build
build:
	mkdir -p $@

build/resources.bundle:
	rucksack bundle assets.json $@ --deps $@.d
all: build/resources.bundle

.PHONY: clean
clean:
	rm -rf build
