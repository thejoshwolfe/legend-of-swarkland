.PHONY: all
all:

OBJECT_NAMES = swarkland.o display.o load_image.o util.o thing.o path_finding.o map.o hashtable.o random.o decision.o tas.o byte_buffer.o item.o input.o event.o string.o text.o resources.o

CPP_FLAGS += -fno-exceptions -fno-rtti -Ibuild/native -Isrc -g -Wall -Wextra -Werror $(MORE_CFLAGS)
COMPILE_CPP = $(CROSS_PREFIX)g++ -c -std=c++11 -o $@ -MMD -MP -MF $@.d $(CPP_FLAGS) $<

LINK_FLAGS += -lm -lrucksack $(MORE_LIBS)
LINK = $(CROSS_PREFIX)gcc -o $@ $^ $(LINK_FLAGS)

# ld is not allowed to omit functions and global variables defined in .o files,
# but it can omit unused content from .a static libraries.
# first make a static library with ar, then make our binary out of that.
LINK_SPARSE = rm -f $@.a; $(foreach f,$^,$(CROSS_PREFIX)ar qcs $@.a $f;) $(CROSS_PREFIX)gcc -o $@ $@.a $(LINK_FLAGS)

THE_VERSION := $(shell echo $$(echo -n $$(cat version.txt)).$$(echo -n $$(git rev-parse --short HEAD)))
# this file has the full version number in its name.
# depend on this whenever you depend on the exact full version number.
VERSION_FILE = build/the_version_is.$(THE_VERSION)
$(VERSION_FILE): version.txt | build
	touch "$@"
build/full_version.txt: $(VERSION_FILE)
	echo -n "$(THE_VERSION)" > $@
%/resources: build/full_version.txt
	rucksack bundle assets.json $(dir $@)resources --deps $@.d
	rucksack strip $(dir $@)resources
%/resources.o: %/resources
	@# the name "resources" is where these symbol names come from:
	@#   _binary_resources_start _binary_resources_end _binary_resources_size
	@# but wait, in windows, the leading underscore is not present, resulting in these names:
	@#    binary_resources_start  binary_resources_end  binary_resources_size
	cd $(dir $@) && $(CROSS_PREFIX)ld -r -b binary resources -o $(notdir $@)

.PHONY: test
test: build/native/test
	build/native/test

.PHONY: native
all: native
-include $(wildcard build/native/*.d)
OBJECTS_native = $(foreach f,$(OBJECT_NAMES),build/native/$f)
$(OBJECTS_native) build/native/main.o build/native/test.o: CROSS_PREFIX =
$(OBJECTS_native) build/native/main.o build/native/test.o: MORE_CFLAGS =
build/native/%.o: src/%.cpp
	$(COMPILE_CPP)

build/native/legend-of-swarkland build/native/test: MORE_LIBS = -lSDL2 -lSDL2_ttf -lpng
build/native/legend-of-swarkland build/native/test: CROSS_PREFIX =
build/native/legend-of-swarkland: $(OBJECTS_native) build/native/main.o
	$(LINK)
build/native/test: $(OBJECTS_native) build/native/test.o
	$(LINK_SPARSE)
native: build/native/legend-of-swarkland

$(OBJECTS_native) build/native/resources: | build/native
build/native:
	mkdir -p $@


# cross compiling for windows.
# See README.md for instructions.
.PHONY: windows
-include $(wildcard build/windows/*.d)
CROSS_windows = $(MXE_HOME)/usr/bin/i686-w64-mingw32.static-
OBJECTS_windows = $(foreach f,$(OBJECT_NAMES),build/windows/$f)
$(OBJECTS_windows) build/windows/main.o: CROSS_PREFIX = $(CROSS_windows)
$(OBJECTS_windows) build/windows/main.o: MORE_CFLAGS = $(shell $(CROSS_PREFIX)pkg-config --cflags SDL2_ttf sdl2 libpng)
build/windows/%.o: src/%.cpp
	$(if $(MXE_HOME),,$(error MXE_HOME is not defined))
	$(COMPILE_CPP)

build/windows/legend-of-swarkland.exe: MORE_LIBS = -static -mconsole $(shell $(CROSS_PREFIX)pkg-config --libs SDL2_ttf sdl2 libpng)
build/windows/legend-of-swarkland.exe: CROSS_PREFIX = $(CROSS_windows)
build/windows/legend-of-swarkland.exe: $(OBJECTS_windows) build/windows/main.o
	$(LINK)
windows: build/windows/legend-of-swarkland.exe

$(OBJECTS_windows) build/windows/resources: | build/windows
build/windows:
	mkdir -p $@


.PHONY: publish-windows
PUBLISH_EXE_NAME = legend-of-swarkland-$(THE_VERSION).exe
FULL_PUBLISH_EXE_NAME = build/publish-windows/$(PUBLISH_EXE_NAME)
publish-windows: $(FULL_PUBLISH_EXE_NAME)

$(FULL_PUBLISH_EXE_NAME): build/windows/legend-of-swarkland.exe | build/publish-windows
	cp $< $@.tmp.exe
	upx $@.tmp.exe
	mv $@.tmp.exe $@
	touch $@

build/publish-windows:
	mkdir -p $@


build:
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf build
