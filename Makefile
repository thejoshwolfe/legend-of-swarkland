.PHONY: all
all:

OBJECT_NAMES = main.o swarkland.o display.o load_image.o util.o individual.o path_finding.o map.o hashtable.o random.o decision.o tas.o byte_buffer.o item.o input.o event.o

CPP_FLAGS += -fno-exceptions -fno-rtti -Ibuild/native -Isrc -g -Wall -Wextra -Werror $(MORE_CFLAGS)
COMPILE_CPP = $(CPP_COMPILER) -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

LINK_FLAGS += -lm -lrucksack $(MORE_LIBS)
LINK = $(LINKER) -o $@ $^ $(LINK_FLAGS)

%/resources.bundle:
	rucksack bundle assets.json $@ --deps $@.d


.PHONY: native
all: native
-include $(wildcard build/native/*.d)
OBJECTS_native = $(foreach f,$(OBJECT_NAMES),build/native/$f)
$(OBJECTS_native): CPP_COMPILER = clang
$(OBJECTS_native): MORE_CFLAGS =
build/native/%.o: src/%.cpp
	$(COMPILE_CPP)

build/native/legend-of-swarkland: MORE_LIBS = -lSDL2 -lSDL2_ttf -lpng
build/native/legend-of-swarkland: LINKER = clang
build/native/legend-of-swarkland: $(OBJECTS_native)
	$(LINK)
native: build/native/legend-of-swarkland
native: build/native/resources.bundle

$(OBJECTS_native): | build/native
build/native:
	mkdir -p $@


# cross compiling for windows.
# See README.md for instructions.
.PHONY: windows
-include $(wildcard build/windows/*.d)
CROSS_windows = $(MXE_HOME)/usr/bin/i686-w64-mingw32.static-
OBJECTS_windows = $(foreach f,$(OBJECT_NAMES),build/windows/$f)
$(OBJECTS_windows): CPP_COMPILER = $(CROSS_windows)g++
$(OBJECTS_windows): MORE_CFLAGS = $(shell $(CROSS_windows)pkg-config --cflags SDL2_ttf sdl2 libpng)
build/windows/%.o: src/%.cpp
	$(if $(MXE_HOME),,$(error MXE_HOME is not defined))
	$(COMPILE_CPP)

build/windows/legend-of-swarkland.exe: MORE_LIBS = -mconsole $(shell $(CROSS_windows)pkg-config --libs SDL2_ttf sdl2 libpng)
build/windows/legend-of-swarkland.exe: LINKER = $(CROSS_windows)g++
build/windows/legend-of-swarkland.exe: $(OBJECTS_windows)
	$(LINK)
windows: build/windows/legend-of-swarkland.exe
windows: build/windows/resources.bundle

$(OBJECTS_windows): | build/windows
build/windows:
	mkdir -p $@


.PHONY: publish-windows
PUBLISH_VERSION := $(shell git rev-parse --short --verify HEAD)
PUBLISH_ZIP_NAME = legend-of-swarkland-$(PUBLISH_VERSION).zip
FULL_PUBLISH_ZIP_NAME = build/publish-windows/$(PUBLISH_ZIP_NAME)
publish-windows: $(FULL_PUBLISH_ZIP_NAME)

PUBLISH_FILES = build/publish-windows/legend-of-swarkland.exe build/publish-windows/resources.bundle
$(FULL_PUBLISH_ZIP_NAME): $(PUBLISH_FILES)
	cd build/publish-windows && zip $(PUBLISH_ZIP_NAME) legend-of-swarkland.exe resources.bundle

build/publish-windows/legend-of-swarkland.exe: build/windows/legend-of-swarkland.exe
	cp $< $@.tmp.exe
	upx $@.tmp.exe
	mv $@.tmp.exe $@
	touch $@

build/publish-windows/resources.bundle: build/windows/resources.bundle
	cp $< $@.tmp
	rucksack strip $@.tmp
	mv $@.tmp $@

$(PUBLISH_FILES): | build/publish-windows
build/publish-windows:
	mkdir -p $@


.PHONY: clean
clean:
	rm -rf build
