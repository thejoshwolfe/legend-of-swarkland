.PHONY: all
all:

-include $(wildcard build/*.d)

OBJECT_NAMES = main.o swarkland.o display.o load_image.o util.o individual.o path_finding.o map.o hashtable.o random.o decision.o tas.o byte_buffer.o item.o input.o event.o

CPP_FLAGS += -Ibuild -Isrc -g -Wall -Werror $(MORE_CFLAGS)
COMPILE_CPP = $(CPP_COMPILER) -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

LINK_FLAGS += -lstdc++ -lm -lrucksack $(MORE_LIBS)
LINK = $(LINKER) -o $@ $^ $(LINK_FLAGS)

%/resources.bundle:
	rucksack bundle assets.json $@ --deps $@.d


.PHONY: native
all: native
OBJECTS_native = $(foreach f,$(OBJECT_NAMES),build/$f)
$(OBJECTS_native): CPP_COMPILER = clang
$(OBJECTS_native): MORE_CFLAGS =
build/%.o: src/%.cpp
	$(COMPILE_CPP)

build/legend-of-swarkland: MORE_LIBS = -lSDL2 -lSDL2_ttf -lfreeimage
build/legend-of-swarkland: LINKER = clang
build/legend-of-swarkland: $(OBJECTS_native)
	$(LINK)
native: build/legend-of-swarkland
native: build/resources.bundle

$(OBJECTS_native): | build
build:
	mkdir -p $@


# cross compiling for windows.
# See README.md for instructions.
.PHONY: windows
CROSS_windows = $(MXE_HOME)/usr/bin/i686-w64-mingw32.static-
OBJECTS_windows = $(foreach f,$(OBJECT_NAMES),build-windows/$f)
$(OBJECTS_windows): CPP_COMPILER = $(CROSS_windows)g++
$(OBJECTS_windows): MORE_CFLAGS = $(shell $(CROSS_windows)pkg-config --cflags SDL2_ttf sdl2 freeimage)
build-windows/%.o: src/%.cpp
	$(if $(MXE_HOME),,$(error MXE_HOME is not defined))
	$(COMPILE_CPP)

build-windows/legend-of-swarkland.exe: MORE_LIBS = -mconsole $(shell $(CROSS_windows)pkg-config --libs SDL2_ttf sdl2 freeimage)
build-windows/legend-of-swarkland.exe: LINKER = $(CROSS_windows)g++
build-windows/legend-of-swarkland.exe: $(OBJECTS_windows)
	$(LINK)
windows: build-windows/legend-of-swarkland.exe
windows: build-windows/resources.bundle

$(OBJECTS_windows): | build-windows
build-windows:
	mkdir -p $@


.PHONY: clean
clean:
	rm -rf build build-windows
