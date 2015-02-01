.PHONY: all
all:

-include $(wildcard build/*.d)

OBJECTS = build/main.o build/swarkland.o build/display.o build/load_image.o build/util.o build/individual.o build/string.o build/path_finding.o

CPP_FLAGS += -Ibuild -Isrc -g -Wall -Werror
COMPILE_CPP = clang -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

build/legend-of-swarkland: $(OBJECTS)
	clang -o $@ -lstdc++ -lSDL2 -lSDL2_ttf -lrucksack -lfreeimage $(OBJECTS)
all: build/legend-of-swarkland

build/%.o: src/%.cpp
	$(COMPILE_CPP)

$(OBJECTS): | build
build:
	mkdir -p $@

build/resources.bundle: assets
	rucksack bundle assets.json $@ --deps $@.d
all: build/resources.bundle

.PHONY: clean
clean:
	rm -rf build
